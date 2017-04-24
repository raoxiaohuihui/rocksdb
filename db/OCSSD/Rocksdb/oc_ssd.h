#ifndef YWJ_OCSSD_H
#define YWJ_OCSSD_H
#include <string>
//DB headers
#include "rocksdb/slice.h"
#include "rocksdb/status.h"
#include "port/port.h"


//liblightnvm headers
#include "liblightnvm.h"
#include "nvm.h"

#include "oc_file.h"

#include <set>

#define BOOL2STR(b) ((b) ? "true" : "false")

namespace rocksdb {
namespace ocssd {

class oc_file;
class oc_block_manager;
class oc_GC;
class oc_page_pool;

class oc_ssd {  //an ocssd device
public:
	oc_ssd();
	~oc_ssd();
	void Setup();
	void Cleanup();
	bool ok()
	{
		return s.ok();
	}

	typedef typename ocssd::oc_file::oc_file_descriptor file_descriptor;
	struct oc_ssd_descriptor {
		const std::string dev_path_;
		std::set<file_descriptor *> files_;
		oc_ssd_descriptor(const char *name) : dev_path_(name)
		{
		}
	};


	inline const struct nvm_geo *Geo()
	{
		return geo_;
	}

	inline oc_block_manager* Blkmng()
	{
		return blkmng_;
	}
	inline oc_page_pool* PagePool()
	{
		return page_pool_;
	}

	//TESTS
	oc_block_manager* TEST_Get_BLK_MNG()
	{
		return blkmng_;
	}
	oc_file* TEST_New_file(const char *fname);

public:
	rocksdb::Status s;

private:
	friend class oc_block_manager;
	friend class oc_GC;

	struct nvm_dev *dev_;
	const struct nvm_geo *geo_;
	int pmode_;
	struct oc_ssd_descriptor *des_;
	oc_block_manager *blkmng_;
	oc_page_pool *page_pool_;


	void EncodeTo(struct oc_ssd_descriptor *ocdes, char *buf);
	void DecodeFrom(struct oc_ssd_descriptor *ocdes, char *buf);

	// No copying allowed
	oc_ssd(const oc_ssd&);
	void operator=(const oc_ssd&);

}; //ocssd


} //namespace ocssd
} //namespace rocksdb

#endif
