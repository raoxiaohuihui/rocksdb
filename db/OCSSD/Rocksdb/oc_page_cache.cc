#include "oc_page_cache.h"
#include "oc_options.h"

#include "rocksdb/slice.h"

#include "util/mutexlock.h"
#include "util/crc32c.h"

#include <cstring>
#include <cassert>
#include <cstdint>
#include <stdlib.h>
#include <malloc.h>
#include <rocksdb/status.h>
#include <rocksdb/env.h>

namespace rocksdb {
namespace ocssd {
const int kPEntry_Degree = 32; //Must Be 32.

static std::string int2str(int x)
{
	char buf[10];
	sprintf(buf, "%d", x);
	return std::string(buf);
}

static void buffill(char *buf, int len)
{
	for (int i = 0; i < len; i++) {
		buf[i] = 'a' + (((i << 2) + i & 0x913af) % 26);
	}
}


oc_page::oc_page(void *mem, int i, oc_page_pool::p_entry *p) : ptr_(mem), ofs_(mem), idx_(i), held_(p)
{
}

oc_page::~oc_page()
{
	if (ptr_) {
		free(ptr_);
	}
}
/*
 * usable range: [ofs_, ptr_ + size_)
 */
size_t oc_page::Append(const char *str, size_t len)
{
	size_t actual = NVM_MIN(Left(), len);
	memcpy(ofs_, str, actual);
	ofs_ = reinterpret_cast<void *>(reinterpret_cast<char *>(ofs_) + actual);
//	assert(size_ >= ofs_ - ptr_);
	return actual;
}

void oc_page::clear()
{
	ofs_ = ptr_;
}

void oc_page::TEST_Info()
{
	printf("%p %p %zu\n", this->ptr_, this->ofs_, this->size_);
}
void oc_page::TEST_Basic()
{
	char buf[1024];
	buffill(buf, 1024);
	buf[1023] = '\0';

	size_t ret = this->Append(buf, 1023);
	ret = this->Append(buf, 1023);
	ret = this->Append(buf, 1023);
	ret = this->Append(buf, 1023);
	ret = this->Append(buf, 1024);

	this->TEST_Info();
	printf("ret: %zu\n", ret);
	printf("write len : %d\n", 1023 * 5 + 1);
	printf("actual len: %zu\n", this->content_len());
	printf("write : [%s] * 5\n", buf);
	printf("actual: %s\n", this->content());
	printf("left:   %zu\n", this->Left());
}


static inline int bitmap2str(uint32_t bitmap, char *buf)
{
	for (int i = 0; i < 32; i++) {
		buf[32 - i - 1] = bitmap & (1 << i) ? '#' : '0';
	}
	buf[32] = '\0';
}

/*
 * gaudent algorithm
 * require 	- only 1 bit of @x is 1. 
 * return 	- the index of this bit.
 * e.g.
 * 0x00000400 --> 10
 */
static inline int bitmap_ntz32(uint32_t x)
{
	int b4, b3, b2, b1, b0;
	b4 = (x & 0x0000ffff) ? 0 : 16;
	b3 = (x & 0x00ff00ff) ? 0 : 8;
	b2 = (x & 0x0f0f0f0f) ? 0 : 4;
	b1 = (x & 0x33333333) ? 0 : 2;
	b0 = (x & 0x55555555) ? 0 : 1;
	return b0 + b1 + b2 + b3 + b4;
}

/*
 * get an empty slot(bit == 0) to use
 */
static int bitmap_get_slot(uint32_t bitmap)
{
	if (~bitmap == 0) {
		return -1; //FULL
	}
	uint32_t x = bitmap | (bitmap + 1); //first bit 0 ==> 1
	x = x & (~bitmap);

	return bitmap_ntz32(x);
}

static inline void bitmap_set(uint32_t *bitmap, int idx)
{
	*bitmap = *bitmap | (1 << idx);
}

static inline void bitmap_unset(uint32_t *bitmap, int idx)
{
	*bitmap = *bitmap & (~(1 << idx));
}


oc_page_pool::oc_page_pool(const struct nvm_geo *g) : ID_(0), geo_(g), page_size_(g->page_nbytes)
{
}

	rocksdb::Status oc_page_pool::New_page_pool(const struct nvm_geo *g,  oc_page_pool **pptr)
{
	oc_page_pool *ptr;
	*pptr = NULL;
	ptr = new oc_page_pool(g);
	if (ptr) {
		*pptr = ptr;
		return rocksdb::Status::OK();
	}
	return rocksdb::Status::IOError("oc_page_pool::New_page_pool failed.");
}

oc_page_pool::~oc_page_pool()
{
	oc_page_pool::p_entry *ptr;
	{
		rocksdb::MutexLock l(&pool_lock_);
		while (!pool_.empty()) {
			ptr = pool_.top();
			Dealloc_p_entry(ptr);
			pool_.pop();
		}
	}
}

void oc_page_pool::TEST_Pr_Usage(const char *title)
{
	char buf[33];
	oc_page_pool::p_entry *ptr;
	std::vector<p_entry *> tmp;
	printf("ID    Usage[%s]\n", title);

	if (pool_.empty()) {
		printf("--NULL--\n");
		return;
	}

	while (!pool_.empty()) {
		ptr = pool_.top();
		bitmap2str(ptr->bitmap_, buf);
		printf("%2d    %02d/32[%s]\n", ptr->id_, ptr->usage_, buf);
		pool_.pop();
		tmp.push_back(ptr);
	}
	for (std::vector<p_entry *>::iterator itr = tmp.begin();
		itr != tmp.end();
		++itr) {
		pool_.push(*itr);
	}
}

void oc_page_pool::TEST_Basic()
{
	oc_page *ptr[65];
	rocksdb::Status s;
	printf("alloc 65 pages-----\n");
	for (int i = 0; i < 65; i++) {
		s = this->AllocPage(&(ptr[i]));
		if (!s.ok()) {
			printf("alloc page failed: %s!\n", int2str(i).c_str());
			return;
		}
		this->TEST_Pr_Usage(int2str(i).c_str());
	}

	printf("dealloc 65 pages-----\n");
	for (int i = 0; i < 65; i++) {
		this->DeallocPage(ptr[i]);
		this->TEST_Pr_Usage(int2str(i).c_str());
	}
}

void oc_page_pool::TEST_Queue()
{
	p_entry * a1,*a2,*a3,*ptr;
	a1 = new p_entry();
	a2 = new p_entry();
	a1->usage_ = 32;
	a2->usage_ = 1;
	pool_.push(a1);
	ptr = pool_.top();
	printf("%d\n", ptr->usage_);

	pool_.push(a2);
	ptr = pool_.top();
	printf("%d\n", ptr->usage_);
}

oc_page_pool::p_entry* oc_page_pool::Alloc_p_entry()
{
	int degree = kPEntry_Degree;
	oc_page_pool::p_entry *ptr = (p_entry *)malloc(sizeof(p_entry) + (degree - 1) * sizeof(oc_page *));
	if (!ptr) {
		s = rocksdb::Status::IOError("oc_page_pool::Alloc_p_entry", strerror(errno));
		return NULL;
	}
	ptr->id_ = ID_;
	ptr->degree_ = degree;
	ptr->usage_ = 0;
	ptr->bitmap_ = 0;
	ID_++;

	for (int i = 0; i < ptr->degree_; i++) {
		void *mem = nvm_buf_alloc(geo_, page_size_);
		if (!mem) {
			s = rocksdb::Status::IOError("oc_page_pool::Alloc_p_entry nvm_buf_alloc", strerror(errno));
			return NULL;
		}
		ptr->reps[i] = new oc_page(mem, i, ptr);
		ptr->reps[i]->size_ = page_size_;
	}

	return ptr;
}

void oc_page_pool::Dealloc_p_entry(oc_page_pool::p_entry *pe)
{
	if (pe) {
		for (int i = 0; i < pe->degree_; i++) {
			delete pe->reps[i];
		}
		free(pe);
	}
}

	rocksdb::Status oc_page_pool::AllocPage(oc_page **pptr)
{
	oc_page_pool::p_entry *ptr = NULL;
	int slot = -1;
	*pptr = NULL;
	bool need_alloc = true;
	{
		rocksdb::MutexLock l(&pool_lock_);

		if (!pool_.empty()) {
			ptr = pool_.top();
			slot = bitmap_get_slot(ptr->bitmap_);
			assert(slot < kPEntry_Degree);
			if (slot >= 0) {
				pool_.pop();
				need_alloc = false;
			}
		}

		if (need_alloc) {
			ptr = Alloc_p_entry();
			if (!s.ok()) {
				goto OUT; //TODO - Refine ?
			}
			slot = bitmap_get_slot(ptr->bitmap_);
		}

		assert(slot >= 0 && slot < kPEntry_Degree);

		//ptr is out of the queue. so don't need to LOCK.
		bitmap_set(&(ptr->bitmap_), slot);
		ptr->usage_++;

		pool_.push(ptr);
	}
	*pptr = ptr->reps[slot];

OUT:
	return s;
}


/*
 * 
 */
void oc_page_pool::DeallocPage(oc_page *p)
{
	{
		rocksdb::MutexLock l(&pool_lock_);
		p->clear();

		bitmap_unset(&(p->held_->bitmap_), p->idx_);
		p->held_->usage_--;
		std::make_heap(const_cast<oc_page_pool::p_entry **>(&pool_.top()),
			const_cast<oc_page_pool::p_entry **>(&pool_.top()) + pool_.size(),
			p_entry_cmp());
	}
}


oc_buffer::~oc_buffer()
{
	clear();
}

void oc_buffer::append(const char *data, size_t len)
{
	size_t wlen = 0;
	rocksdb::Status s;
	int itr = 0;
	while (wlen < len) {
		if (active_) {
			wlen += active_->Append(data + wlen, len - wlen);
//			printf("[oc_buffer] - len: %zu; has write: %zu; itr: %d\n", len, wlen, itr);
			if (wlen == len) {
				size_ += len;
				return;
			}
			todump_.push_back(active_);
		}

		s = page_pool_->AllocPage(&active_);
		if (!s.ok()) {
			printf("[oc_buffer] %s\n", s.ToString().c_str());
			abort();
		}
		itr++;
	}
}

void oc_buffer::push_active()
{
	if (active_) {
		todump_.push_back(active_);
		active_ = NULL;
	}
}

void oc_buffer::clear()
{
	push_active();
	cleanup();
}

void oc_buffer::cleanup()
{
	size_ = 0;
	for (dump_iterator itr = todump_.begin();
		itr != todump_.end();
		++itr) {
		page_pool_->DeallocPage(*itr);
	}
	todump_.clear();
}
	rocksdb::Status oc_buffer::dump2file()
{
	rocksdb::Status s;
	oc_page *ptr;
	push_active();
//	printf("vector_size: %zu\n", todump_.size());
	printf("\n");
	for (dump_iterator itr = todump_.begin();
		itr != todump_.end();
		++itr) {
		ptr = *itr;
		size_t i = 0;
		while (i < ptr->content_len()) {
			printf("%c", *(ptr->content() + i));
			i++;
		}
	}
	printf("\n");
	return s;
}

	rocksdb::Status oc_buffer::dump2file(oc_file *f)
{
	rocksdb::Status s;
	return s;
}

	rocksdb::Status oc_buffer::dump2file(rocksdb::WritableFile *f)
{
	rocksdb::Status s;
	oc_page *ptr;
	push_active();
	for (dump_iterator itr = todump_.begin();
		itr != todump_.end();
		++itr) {
		ptr = *itr;
		s = f->Append(rocksdb::Slice(ptr->content(), ptr->content_len()));
		if (!s.ok()) {
			break;
		}
	}
	return s;
}

void oc_buffer::update_pagesinfo()
{
	oc_page *ptr;
	char buf[200];
	info_.clear();
	push_active();
	snprintf(buf, 200, "[oc_buffer pages info]\n number of pages: %zu, ", todump_.size());
	info_.append(buf);
	for (dump_iterator itr = todump_.begin();
		itr != todump_.end();
		++itr) {
		ptr = *itr;
		snprintf(buf, 200, " id%d(in_used_bytes:%zu),", ptr->idx_, ptr->content_len()); 
		info_.append(buf);
	}
	info_.append("\n");
}
void oc_buffer::pr_pagesinfo()
{
	update_pagesinfo();
	printf("pages info\n %s",info_.c_str());
}
//TESTS
void oc_buffer::TEST_Basic()
{
	char buf[1024];
	buffill(buf, 1024);
	buf[11] = '\0';
	this->append(buf, 10);
//	this->append(buf, 10);
//	this->append(buf, 10);
	this->append(buf + 10, 1);
	printf("oc_buffer_size: %zu\n",  this->size());

	printf("write:  %s\n", buf);
	printf("actual: ");
	this->dump2file();
	printf("\n");
}

void oc_buffer::TEST_Large()
{
	char buf[1024];
	buffill(buf, 1024);
	buf[999] = '\0';
	this->append(buf, 1000);
	this->append(buf, 1000);
	this->append(buf, 1000);
	this->append(buf, 1000);
	this->append(buf, 1000);
	printf("oc_buffer_size: %zu\n",  this->size());

	printf("write:  %s\n", buf);
	printf("actual: ");
	this->dump2file();
	printf("\n");
}

void oc_buffer::TEST_Large2()
{
	char *buf = (char *)malloc(sizeof(char) * 8000);
	buffill(buf, 8000);
	buf[7999] = '\0';

	this->append(buf, 8000);
	printf("oc_buffer_size: %zu\n",  this->size());

	printf("write:  %s\n", buf);
	printf("actual: ");
	this->dump2file();
	printf("\n");
	free(buf);
}

void oc_buffer::TEST_WritableFile()
{
//	rocksdb::WritableFile *file;
//	rocksdb::Env *env = rocksdb::Env::Default();
//	rocksdb::Status s = env->NewWritableFile("oc_buffer::TEST_WritableFile.out", &file);
//	if (!s.ok()) {
//		printf("New file failed.\n");
//		return;
//	}
//
//	char buf2[100];
//	char *buf = (char *)malloc(sizeof(char) * 8000);
//	buffill(buf, 8000);
//	buf[7999] = '\0';
//
//	this->append(buf, 8000);
//
//	snprintf(buf2, 100, "oc_buffer_size: %zu\nwrite:  ",  this->size());
//	file->Append(rocksdb::Slice(buf2));
//	file->Append(rocksdb::Slice(buf));
//	file->Append("\n");
//	file->Append("actual: ");
//	this->dump2file(file);
//	file->Append("\n");
//	file->Flush();
//	file->Close();
//
//	free(buf);
}

void oc_buffer::TEST_WritableFile_Clear_Again()
{
//	rocksdb::WritableFile *file;
//	rocksdb::Env *env = rocksdb::Env::Default();
//	rocksdb::Status s = env->NewWritableFile("oc_buffer::TEST_WritableFile.out", &file);
//	if (!s.ok()) {
//		printf("New file failed.\n");
//		return;
//	}
//
//	char *buf = (char *)malloc(sizeof(char) * 20);
//	buffill(buf, 20);
//	buf[19] = '\0';
//
//	printf("Append 20 bytes====\n");
//	this->append(buf, 20);
//
//	this->pr_pagesinfo();
//	printf("oc_buffer size: %zu", this->size());
//	this->dump2file();
//
//	printf("Clear====\n");
//	this->clear();
//
//	this->pr_pagesinfo();
//	printf("oc_buffer size: %zu", this->size());
//	this->dump2file();
//
//	printf("Append 20 bytes====\n");
//	this->append(buf, 20);
//
//	this->pr_pagesinfo();
//	printf("oc_buffer size: %zu", this->size());
//	this->dump2file();
}


uint32_t oc_buffer::CRCValue()
{
	oc_page *ptr;
	uint32_t crc;
	dump_iterator itr;

	push_active();

	itr = todump_.begin();
	ptr = *itr;
	crc = rocksdb::crc32c::Value(ptr->content(), ptr->content_len());
	++itr;

	for (;
		itr != todump_.end();
		++itr) {
		ptr = *itr;
		crc = rocksdb::crc32c::Extend(crc, ptr->content(), ptr->content_len());
	}
	return crc;
}

} //namespace ocssd
} //namespace rocksdb
