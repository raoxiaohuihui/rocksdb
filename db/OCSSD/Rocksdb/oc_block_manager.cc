#include "oc_ssd.h"
#include "oc_block_manager.h"

#include "util/mutexlock.h"
#include "utils/common.h"

#include <cassert>
#include <set>

namespace rocksdb {
namespace ocssd {

namespace {
const oc_block_manager::BlkState_t BLK_UNUSED = NVM_BBT_FREE;
const oc_block_manager::BlkState_t BLK_USED = NVM_BBT_HMRK;
const oc_block_manager::BlkState_t BLK_INVALID = NVM_BBT_GBAD;
};

static inline int _ch_lun2_bbt_idx(int ch, int lun, const struct nvm_geo *geo)
{
	return ch * geo->nluns + lun;
}

static inline int _pl_blk2_bbtblks_idx(int pl, int blk, const struct nvm_geo *geo)
{
	return blk * geo->nplanes + pl;
}

static void my_nvm_bbt_state_pr(int state)
{
	switch (state) {
	case BLK_UNUSED:
		printf("B_00(%d)", state);
		break;
	case BLK_USED:
		printf("B_##(%d)", state);
		break;
	case BLK_INVALID:
		printf("B_!!(%d)", state);
		break;
	default:
		nvm_bbt_state_pr(state);
		break;
	}
}

static void my_nvm_bbt_state_pr_verbose(int state, FILE *fp = stdout)
{
	switch (state) {
	case BLK_UNUSED:
		fprintf(fp, "00");
		break;
	case BLK_USED:
		fprintf(fp, "##");
		break;
	case BLK_INVALID:
		fprintf(fp, "!!");
		break;
	default:
		fprintf(fp, "NA");
		break;
	}
}



oc_block_manager::oc_blk_mng_descriptor::oc_blk_mng_descriptor(oc_ssd *ssd) throw()
	: chunk_grain_factor(16),
	  extent_alloc_policy(oc_options::kFirstFit),
	  bbt_cached(true),
	  erase_on_startup(false),
	  ssd_(ssd),
	  geo_(ssd_->Get_geo()),
	  bbts_(NULL),
	  bbts_length_(-1),
	  blks_length_(-1)
{
	alloc_unit_in_bytes = geo_->npages * geo_->page_nbytes / chunk_grain_factor; //To be Refine
}

oc_block_manager::oc_blk_mng_descriptor::~oc_blk_mng_descriptor() throw()
{
}



oc_block_manager::oc_block_manager(oc_ssd *ssd) throw(bbt_operation_not_good, erase_block_not_good)
	: des_(new oc_block_manager::oc_blk_mng_descriptor(ssd))
{
	assert(des_->bbt_cached); //now only support bbt cached.
	if (des_->bbt_cached) {
		try {
			init_bbts();
		} catch (const bbt_operation_not_good & e) {
			delete des_;
			throw e;
		} catch (const std::exception & e) {
			delete des_;
			throw e;
		}
	}
}

oc_block_manager::~oc_block_manager() throw()
{
	delete des_;
}


void oc_block_manager::init_bbts() throw(bbt_operation_not_good)
{
	size_t i;
	struct nvm_addr lun_addr;
	struct nvm_ret ret;
	const struct nvm_bbt *ptr;
	std::string err;
	
	if (nvm_dev_set_bbts_cached(des_->ssd_->Get_dev(), 1)) {
		throw bbt_operation_not_good("nvm_dev_set_bbts_cached");
	}

	for (i = 0; i < des_->geo_->nluns; i++) {
		lun_addr.ppa = 0;
		lun_addr.g.lun = i;
		ptr = nvm_bbt_get(des_->ssd_->Get_dev(), lun_addr, &ret);
		if (!ptr) {
			err = "nvm_bbt_get lun ";
			StrAppendInt(err, i); 
			throw bbt_operation_not_good(err.c_str()); 
		}
	}

	des_->bbts_ = des_->ssd_->Get_dev()->bbts;
	des_->bbts_length_ = des_->geo_->nchannels * des_->geo_->nluns; //sum of the LUNs.
	des_->blks_length_ = des_->geo_->nplanes * des_->geo_->nblocks; //sum of the blks inside a LUNs.
}

void oc_block_manager::print_info()
{
	printf("-----oc_block_manager's configuration:-----\n");
	printf("chunk_grain_factor: %d\n",  des_->chunk_grain_factor);
	printf("alloc_unit_in_bytes: %d\n",  des_->alloc_unit_in_bytes);
	printf("extent_alloc_policy: %s\n", des_->extent_alloc_policy == oc_options::kFirstFit ? "FirstFit" : "other");
	printf("bbt_cached: %s\n",  BOOL2STR(des_->bbt_cached));
	printf("erase_on_startup: %s\n",  BOOL2STR(des_->erase_on_startup));
}

void oc_block_manager::print_bbts(bool pr2file)
{
	size_t ch, b, l, p;
	int bbt_idx, blk_idx;
	FILE* fp;
	std::string info;
	if (!pr2file || !(fp = fopen("badblocktable.txt", "w+"))) {
		fp = stdout;
	}
	StrAppendTime(info);
	fprintf(fp, "Block State Table(%s)\n", info.c_str());
	//print title.
	for (ch = 0; ch < des_->geo_->nchannels; ++ch) {
		fprintf(fp, "       ");
		for (l = 0; l < des_->geo_->nluns; ++l) {
			fprintf(fp, "L%zu     ", l);
		}
		fprintf(fp, "\n");

		for (b = 0; b < des_->geo_->nblocks; ++b) {
			fprintf(fp, "B%4zu: ", b);
			for (l = 0; l < des_->geo_->nluns; ++l) {
				for (p = 0; p < des_->geo_->nplanes; ++p) {
					bbt_idx = _ch_lun2_bbt_idx(ch, l, des_->geo_);
					blk_idx = _pl_blk2_bbtblks_idx(p, b, des_->geo_);
					my_nvm_bbt_state_pr_verbose(des_->bbts_[bbt_idx]->blks[blk_idx], fp);
					fprintf(fp, " ");
				}
				fprintf(fp, " ");
			}
			fprintf(fp, "\n");
		}
	}
	if (fp != stdout) {
		fclose(fp);
	}
}



/*
 * 
 */
void oc_block_manager::erase_on_startup() throw()
{

}


void oc_block_manager::flush_bbts() throw(bbt_operation_not_good)
{
	///WARNING - Flush the BBT will cause it free the bbt entry.
	struct nvm_ret ret;
	if (nvm_bbt_flush_all(des_->ssd_->Get_dev(), &ret)) {
		throw bbt_operation_not_good("nvm_bbt_flush_all");
	}
}

} //namespace ocssd
} //namespace rocksdb
