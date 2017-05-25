#ifndef YWJ_OCSSD_OC_BLK_MNG_H
#define YWJ_OCSSD_OC_BLK_MNG_H

#include "oc_options.h"
#include "oc_gc.h"

//mutex & condition varieble headers
#include "port/port.h"


//liblightnvm headers
#include "liblightnvm.h"
#include "nvm.h"

#include "utils/common.h"
#include "utils/oc_exception.hpp"

#include <cstddef>
#include <cstdint>
#include <rocksdb/status.h>


namespace rocksdb {
namespace ocssd {

class oc_ssd;
class oc_GC;


class oc_blk_cleaner {
public:
	oc_blk_cleaner(bool erase_on_startup, bool bbtcached) throw(blk_cleaner_exception);
	~oc_blk_cleaner() throw();

	void ReadBadBlockTable() throw(blk_cleaner_exception);
	void StoreBadBlockTable() throw(blk_cleaner_exception);

	//issue GC request

private:
	struct nvm_bbt **bbts_;
	int bbts_length_;
	int blks_length_;
};


class oc_block_manager { //allocation is done in a granularity of <Block>
public:
	oc_block_manager(oc_ssd *ssd) throw(blk_cleaner_exception);
	~oc_block_manager() throw();

	void Info(std::string& str);

private:

	typedef int BlkState_t;

	struct oc_blk_mng_descriptor {
		/* 
		 *	This decide the file's allocation unit & extent-inner bimap size:  
		 *		alloc_unit_in_bytes = 1 / <chunk_grain_factor> * <Block_Bytes>, 
		 *  	bit numbers of the extent bitmap = chunk_grain_factor,
		 *	Default: 16 
		 */
		int chunk_grain_factor;
		int alloc_unit_in_bytes;

		/* 
		 *  Default: kFirstFit
		 */
		oc_options::ExtentAllocPolicy policy;

		/*
		 *	Default: true
		 */
		bool bbt_cached;

		/*
		 *	Default: false
		 */
		bool erase_on_startup;

		//other fields
		oc_ssd *const ssd_;
		const struct nvm_geo *geo_;

		oc_blk_mng_descriptor(oc_ssd *ssd) throw();
		~oc_blk_mng_descriptor() throw();
	};




	friend class oc_ssd;
	friend class oc_GC;

	oc_blk_mng_descriptor *des_;
	oc_blk_cleaner *clnr_;


	void InitClean();
	void InitBBTs();
	void FlushBBTs();
	void Init();

	void Add_blks(size_t blks);
	void Set_stripe_blks_as(struct StripeDes des, BlkState_t flag);


	/*
	 * oc_block_manager factory function
	 */
	static rocksdb::Status New_oc_block_manager(oc_ssd *ssd,  oc_block_manager **oc_blk_mng_ptr);

};

}
}
#endif
