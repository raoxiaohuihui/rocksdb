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

#include "utils/common.hpp"

#include "oc_file.h"

#include <set>


namespace rocksdb {
namespace ocssd {

class oc_file;
class oc_block_manager;
class oc_GC;
class oc_page_pool;

/* 
 * OCSSD device. 
 * The entrance of OCSSD Impl. 
 *  
 * ---------- Refine Note ----------------
 * 1. use exception to handle error. 
 * 2. 
 * ---------- TODO ------------------------
 */


class oc_ssd {  
public:
	oc_ssd();
	~oc_ssd();


	struct oc_ssd_descriptor {
		const std::string dev_path_;
		oc_ssd_descriptor(const char *path) : dev_path_(path)
		{
		}
	};

	inline bool ok() {
		return s.ok();
	}
	inline const struct nvm_geo *Get_geo(){
		return geo_;
	}
	inline oc_block_manager* Get_Blkmng(){
		return blkmng_;
	}
	inline oc_page_pool* Get_Pgpool(){
		return page_pool_;
	}

	//TESTS
	oc_file* TEST_New_file(const char *fname);

private:
	friend class oc_block_manager;
	friend class oc_GC;

	rocksdb::Status s;

	void init();
	void deinit();

	struct oc_ssd_descriptor *des_;
	struct nvm_dev *dev_;
	const struct nvm_geo *geo_;
	oc_block_manager *blkmng_;
	oc_page_pool *pgp_;


	void EncodeTo(struct oc_ssd_descriptor *ocdes, char *buf);
	void DecodeFrom(struct oc_ssd_descriptor *ocdes, char *buf);

	// No copying allowed
	oc_ssd(const oc_ssd&);
	void operator=(const oc_ssd&);

}; //ocssd


} //namespace ocssd
} //namespace rocksdb

#endif
