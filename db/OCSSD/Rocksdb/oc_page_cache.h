#ifndef YWJ_OCSSD_OC_PAGE_CACHE_H
#define YWJ_OCSSD_OC_PAGE_CACHE_H

//liblightnvm headers
#include "liblightnvm.h"
#include "nvm.h"

#include "rocksdb/status.h"
#include "rocksdb/env.h"

//mutex & condition varieble headers
#include "port/port.h"



#include <queue>
#include <vector>

namespace rocksdb {
namespace ocssd {


class oc_ssd;
class oc_page;
class oc_file;

class oc_page_pool {
public:
	~oc_page_pool();
	rocksdb::Status AllocPage(oc_page **pptr);
	void 			DeallocPage(oc_page *p);

	//TESTS
	void TEST_Pr_Usage(const char *title);
	void TEST_Queue();
	void TEST_Basic();


private:

	struct p_entry {
		int id_;
		int degree_;
		int usage_;
		uint32_t bitmap_;
		oc_page *reps[1];
	};

	struct p_entry_cmp {
		bool operator ()(p_entry *a, p_entry *b)
		{
			//printf("CALLED! %d %d, Ret:%d\n",a->usage_ , b->usage_, a->usage_ < b->usage_);
			return a->usage_ > b->usage_;
		}
	};

	friend class oc_ssd;
	friend class oc_page;

	oc_page_pool(const struct nvm_geo *g);
	static rocksdb::Status 	New_page_pool(const struct nvm_geo *g, oc_page_pool **pptr);
	p_entry*				Alloc_p_entry();
	void 					Dealloc_p_entry(p_entry *pe);

	std::priority_queue<p_entry *, std::vector<p_entry *>, p_entry_cmp> pool_;
	rocksdb::port::Mutex pool_lock_;

	int 						ID_;	
	const struct nvm_geo *const geo_;
	const size_t 				page_size_;
	rocksdb::Status 			s;
};


class oc_page {
public:
	size_t Append(const char *str, size_t len);
	inline size_t Left()
	{
		return reinterpret_cast<char *>(ptr_) + size_ - reinterpret_cast<char *>(ofs_);
	}
	inline size_t content_len()
	{
		return reinterpret_cast<char *>(ofs_) - reinterpret_cast<char *>(ptr_);
	}
	inline const char* content()
	{
		return reinterpret_cast<const char *>(ptr_);
	}
	
	~oc_page();

	//TESTS
	void TEST_Info();
	void TEST_Basic();

private:
	friend class oc_page_pool;
	friend class oc_buffer;
	void clear();

	oc_page_pool::p_entry *held_;
	void*		ptr_;
	void*		ofs_;
	int 		idx_;
	size_t 		size_;

	oc_page(void *mem, int i, oc_page_pool::p_entry *p);

	//no copy
	oc_page(oc_page const&);
	const oc_page operator=(oc_page const&);
};

/* 
 * a wrapper to oc_page. 
 * Support: I/O to oc_file, I/O to writablefile 
 * Not Support: compress
 */
class oc_buffer {
public:
	oc_buffer(oc_page_pool *p) : page_pool_(p), size_(0), active_(NULL) { }
	~oc_buffer();
	void append(const char *data, size_t len);
	void clear();
	
	void pr_pagesinfo();

	inline bool empty()
	{
		return size_ == 0;
	}
	inline size_t size()
	{
		return size_;
	}

	inline std::string toString(){
		std::string toString(active_->content());
		return toString;
	}

	rocksdb::Status dump2file();
	rocksdb::Status dump2file(oc_file *f);
	rocksdb::Status dump2file(rocksdb::WritableFile *f);

	uint32_t CRCValue();

	//TESTS
	void TEST_Basic();
	void TEST_Large();
	void TEST_Large2();
	void TEST_WritableFile();
	void TEST_WritableFile_Clear_Again();
private:
	typedef std::vector<oc_page *>::iterator dump_iterator;
	void push_active();
	void cleanup();
	void update_pagesinfo();

	oc_page*				active_;
	oc_page_pool*			page_pool_;
	std::vector<oc_page *> 	todump_;
	size_t 					size_;
	std::string				info_;
};

} //namespace ocssd
} //namespace rocksdb


#endif
