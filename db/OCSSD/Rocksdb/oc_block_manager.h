#ifndef YWJ_OCSSD_OC_BLK_MNG_H
#define YWJ_OCSSD_OC_BLK_MNG_H

#include "oc_options.h"
#include "oc_gc.h"

//mutex & condition varieble headers
#include "port/port.h"


//liblightnvm headers
#include "liblightnvm.h"
#include "nvm.h"

#include <cstddef>
#include <cstdint>
#include <rocksdb/status.h>


namespace rocksdb {
namespace ocssd {

class oc_ssd;
class oc_GC;

class oc_block_manager { //allocation is done in a granularity of <Block>
public:


	typedef uint32_t LunAndPlane_t;
	typedef uint8_t BlkState_t;

	struct rr_addr { //RoundRobin address format
		LunAndPlane_t lap;
		uint32_t block;
		rr_addr() : lap(0), block(0){ }

		bool ok(const struct nvm_geo *limit);
		rr_addr& Increment(const struct nvm_geo *limit);
		rr_addr& Increment(size_t blks, const struct nvm_geo *limit);
		size_t Minus(const rr_addr& rhs, const struct nvm_geo *limit);
		
		bool operator<=(const rr_addr& rhs);
		rr_addr& operator=(const rr_addr& rhs);
	};

	struct Itr_rr_addr { //Iterator of RoundRobin address
		size_t blks;
		struct rr_addr st;
		struct rr_addr ed;
		const struct nvm_geo *limit;
		Itr_rr_addr(struct rr_addr s, struct rr_addr e, const struct nvm_geo *g): blks(e.Minus(s, g)), st(s), ed(e), limit(g) { }
		void SetBBTInCache(struct nvm_bbt **bbts, BlkState_t flag); 	//Increment by 1(Memory Operation)
		

		rocksdb::Status Write();										//Increment by stripe(partial stripe)
		rocksdb::Status Read();											//Increment by stripe(partial stripe)
		void TEST_Iteration();											//For Unit TEST
	};

	struct StripeDes {	//Stripe Descriptor: an parellel unit, consist of blocks, the blocks is [st, ed)
		struct rr_addr st;
		struct rr_addr ed;
		StripeDes() : st(), ed(){ }
		StripeDes(struct rr_addr s, struct rr_addr e) : st(s), ed(e){ }
	};

	//REQUIRE - bytes should be multiple of <chunk_size>.
	rocksdb::Status AllocStripe(size_t bytes, StripeDes *sd);
	rocksdb::Status FreeStripe(StripeDes *sd);
	rocksdb::Status FreeStripeArray(StripeDes *sds, int num);


	bool ok();
	void Pr_BlocksState(const char *fname = 0);


	//TESTS
	void TEST_Pr_Opt_Meta();
	rocksdb::Status TEST_Pr_BBT();
	

	void TEST_Lap();
	void TEST_Add();
	void TEST_Pr_UM();	
	void TEST_Itr_rr_addr();

	inline size_t ChunkSize()
	{
		return opt_.chunk_size;
	}

	size_t TEST_Get_ChunkSize()
	{
		return opt_.chunk_size;
	}

	static void TEST_My_nvm_bbt_pr(int lun, const struct nvm_bbt *bbt, const char *fname = 0);
	static void TEST_RR_Addr_Pr(struct rr_addr x); 

private:
	/*  Problems to be issued:
	 *  1. a file can be discontinuous in lun : (a file's size is changable at any time: support Append as a FS?)
	 *  2. chunk size is flexible to each incoming request or fixed by a definition before ?
	 * 
	 */
	struct Options {
		size_t chunk_size;          				//default: a block's size in bytes.
		oc_options::AddrAllocPolicy policy;			//default: kRoundRobin_Fixed(an allocation will alloc the same chunks for each plane.)
		bool bbt_cached;            				//default: enabled
	};



	friend class oc_ssd;
	friend class oc_GC;

	void def_ocblk_opt(struct Options *opt);

	void InitClean();
	void InitBBTs();
	void FlushBBTs();
	void Init();

	void Add_blks(size_t blks);
	void Set_stripe_blks_as(struct StripeDes des, BlkState_t flag);

	oc_block_manager(oc_ssd *ssd, const struct nvm_geo *g);

	/*
	 * oc_block_manager factory function
	 */
	static rocksdb::Status New_oc_block_manager(oc_ssd *ssd,  oc_block_manager **oc_blk_mng_ptr);

	oc_ssd *const ssd_;
	const struct nvm_geo * geo_;
	struct nvm_bbt **bbts_;
	int bbts_length_;
	int blks_length_;
	rocksdb::port::Mutex bbts_lock;


	struct rr_addr next;
	rocksdb::port::Mutex next_lock;

	struct Options opt_;
	rocksdb::Status s;
};

}
}
#endif
