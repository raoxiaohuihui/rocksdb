#include "oc_page_cache.h"
#include "oc_options.h"

#include "rocksdb/slice.h"

#include "util/mutexlock.h"
#include "util/crc32c.h"

#include "utils/common.h"

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



static void buffill(char *buf, int len)
{
	for (int i = 0; i < len; i++) {
		buf[i] = 'a' + (((i << 2) + (i & 0x913af)) % 26);
	}
}


oc_page::oc_page(void *mem, int i, oc_page_pool::p_entry *p) 
:  held_(p), 
	ptr_(mem), 
	ofs_(mem), 
	idx_(i)
{
}

oc_page::~oc_page()
{
	if (ptr_) {
		free(ptr_);
	}
}

oc_page_pool::oc_page_pool(const struct nvm_geo *g)
	: ID_(0),
	  geo_(g),
	  page_size_(g->page_nbytes),
	  degree_(kPEntry_Degree)
{
}

oc_page_pool::~oc_page_pool()
{
	oc_page_pool::p_entry *ptr;
	{
		rocksdb::MutexLock l(&pool_lock_);
		while (!pool_.empty()) {
			ptr = pool_.top();
			dealloc_p_entry(ptr);
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
	std::string str;
	oc_page *ptr[65];
	rocksdb::Status s;
	printf("alloc 65 pages-----\n");
	for (int i = 0; i < 65; i++) {
		this->page_alloc(&(ptr[i]));
		if (!s.ok()) {
			printf("alloc page failed: %s!\n", StrAppendInt(str, i).c_str());
			return;
		}
		str.clear();
		this->TEST_Pr_Usage(StrAppendInt(str, i).c_str());
	}

	printf("dealloc 65 pages-----\n");
	for (int i = 0; i < 65; i++) {
		this->page_dealloc(ptr[i]);
		str.clear();
		this->TEST_Pr_Usage(StrAppendInt(str, i).c_str());
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

oc_page_pool::p_entry* oc_page_pool::alloc_p_entry()
{
	oc_page_pool::p_entry *ptr = (p_entry *)malloc(sizeof(p_entry) + (degree_ - 1) * sizeof(oc_page *));
	if (!ptr) {
		s_ = rocksdb::Status::IOError("oc_page_pool::alloc_p_entry");
		throw page_pool_exception(s_.ToString().c_str()); 
	}
	ptr->id_ = ID_;
	ptr->usage_ = 0;
	ptr->bitmap_ = 0;

	{
		rocksdb::MutexLock l(&meta_lock_);
		ID_++;
	}

	for (int i = 0; i < degree_; i++) {
		void *mem = nvm_buf_alloc(geo_, page_size_);
		if (!mem) {
			s_ = rocksdb::Status::IOError("oc_page_pool::Alloc_p_entry nvm_buf_alloc", strerror(errno));
			throw page_pool_exception(s_.ToString().c_str()); 
		}
		ptr->reps[i] = new oc_page(mem, i, ptr);
	}

	return ptr;
}

void oc_page_pool::dealloc_p_entry(oc_page_pool::p_entry *pe)
{
	if (pe) {
		for (int i = 0; i < degree_; i++) {
			delete pe->reps[i];
		}
		free(pe);
	}
}

void oc_page_pool::page_alloc(oc_page **pptr) throw(page_pool_exception)
{
	oc_page_pool::p_entry *ptr = NULL;
	int slot = -1;
	*pptr = NULL;
	if (!pool_.empty()) 
	{
		{
			rocksdb::MutexLock l(&pool_lock_);
			ptr = pool_.top();
			if (bitmap_is_all_set(ptr->bitmap_)) {
				ptr = NULL;
			} else {
				pool_.pop();
			}
		}
		if (ptr) {
			rocksdb::MutexLock l(&ptr->bm_lock_); 
			slot = bitmap_get_slot(ptr->bitmap_);
		}
	}

	if (!ptr) {
		try {
			ptr = alloc_p_entry();
		} catch (const page_pool_exception & e) {
			throw e;
		} catch (const std::exception & e) {
			throw e;
		}
		slot = 1;
	}
	{
		rocksdb::MutexLock l(&ptr->bm_lock_);
		bitmap_set(&(ptr->bitmap_), slot);
	}
	ptr->usage_++;
	{
		rocksdb::MutexLock l(&pool_lock_);
		pool_.push(ptr);
	}
	*pptr = ptr->reps[slot];
}


/*
 * 
 */
void oc_page_pool::page_dealloc(oc_page *p) throw()
{
	this->page_clear(p);

	{
		rocksdb::MutexLock l(&pool_lock_);
		{
			rocksdb::MutexLock(&p->held_->bm_lock_);
			bitmap_unset(&(p->held_->bitmap_), p->idx_);
		}	
		p->held_->usage_--;
		std::make_heap(const_cast<oc_page_pool::p_entry **>(&pool_.top()),
			const_cast<oc_page_pool::p_entry **>(&pool_.top()) + pool_.size(),
			p_entry_cmp());
	}
}

/*
 * an oc_page's usable range: [p->ofs_, p->ptr_ + size_)
 */
size_t oc_page_pool::page_append(oc_page *p, const char *str, size_t len)
{
	size_t actual = NVM_MIN(this->page_leftbytes(p), len); 
	memcpy(p->ofs_, str, actual);
	p->ofs_ = reinterpret_cast<void *>(reinterpret_cast<char *>(p->ofs_) + actual);
//	assert(size_ >= ofs_ - ptr_);
	return actual;
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
			wlen += page_pool_->page_append(active_, data + wlen, len - wlen); 
//			printf("[oc_buffer] - len: %zu; has write: %zu; itr: %d\n", len, wlen, itr);
			if (wlen == len) {
				size_ += len;
				return;
			}
			todump_.push_back(active_);
		}
		try {
			page_pool_->page_alloc(&active_);
		} catch (const page_pool_exception & e) {
			throw e;
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
		page_pool_->page_dealloc(*itr); 
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
		while (i < page_pool_->page_content_len(ptr)) {
			printf("%c", *(page_pool_->page_content(ptr) + i));
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
		s = f->Append(rocksdb::Slice(page_pool_->page_content(ptr), page_pool_->page_content_len(ptr)));
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
		snprintf(buf, 200, " id%d(in_used_bytes:%zu),", ptr->idx_, page_pool_->page_content_len(ptr)); 
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
	crc = rocksdb::crc32c::Value(page_pool_->page_content(ptr), page_pool_->page_content_len(ptr)); 
	++itr;

	for (;
		itr != todump_.end();
		++itr) {
		ptr = *itr;
		crc = rocksdb::crc32c::Extend(crc, page_pool_->page_content(ptr), page_pool_->page_content_len(ptr));
	}
	return crc;
}

} //namespace ocssd
} //namespace rocksdb
