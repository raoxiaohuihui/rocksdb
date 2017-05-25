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

oc_blk_cleaner::oc_blk_cleaner(bool erase_on_startup, bool bbtcached)
{
     
}


oc_block_manager::oc_blk_mng_descriptor::oc_blk_mng_descriptor(oc_ssd *ssd)
	: chunk_grain_factor(16),
	  policy(oc_options::kFirstFit),
	  bbt_cached(true),
	  erase_on_startup(false),
	  ssd_(ssd),
	  geo_(ssd_->Get_geo())
{
	alloc_unit_in_bytes = geo_->npages * geo_->page_nbytes / chunk_grain_factor;
}




oc_block_manager::oc_block_manager(oc_ssd *ssd)
try : des_(new oc_block_manager::oc_blk_mng_descriptor(ssd)),   
	clnr_(new oc_block_manager::oc_blk_cleaner(des_->erase_on_startup, des_->bbt_cached)) 
{
} catch (const blk_cleaner_exception & e){
	delete des_;
	throw e;
} catch (const std::exception & e){
	throw e;
}

oc_block_manager::~oc_block_manager()
{
	delete des_;
	delete clnr_;
}



void oc_block_manager::Info()
{
	printf("BLKMNG Opts:\n");
	printf("bbt_cached: %s.\n"
		"addralloc: %s.\n"
		"chunk_size: %zu.\n",
		BOOL2STR(opt_.bbt_cached),
		opt_.policy == oc_options::kRoundRobin_Fixed ? "RoundRobin_Fixed" : "Other",
		opt_.chunk_size);
	printf("BLKMNG Init Meta:\n");
	printf("Next LAP: <L%dP%d> Block:%u\n", GetLun(next.lap), GetPlane(next.lap), next.block);
}

//Block-lun-plane addressing
void oc_block_manager::Pr_BlocksState(const char *fname)
{
	size_t b, l, p;
	int bbt_idx, blk_idx;
	FILE *fp;
	std::string info;
	if (fname && !(fp = fopen(fname, "w+"))) {
		fp = stdout;
	}
	//print infos
	gettimestr(info);
	fprintf(fp, "Block State Table(%s)\n", info.c_str());
	//print title.
	fprintf(fp, "       ");
	for (l = 0; l < geo_->nluns; ++l) {
		fprintf(fp, "L%zu     ", l);
	}
	fprintf(fp, "\n");

	for (b = 0; b < geo_->nblocks; ++b) {
		fprintf(fp, "B%4zu: ", b);
		for (l = 0; l < geo_->nluns; ++l) {
			for (p = 0; p < geo_->nplanes; ++p) {
				bbt_idx = _ch_lun2_bbt_idx(CH_THIS, l, geo_);
				blk_idx = _pl_blk2_bbtblks_idx(p, b, geo_);
				my_nvm_bbt_state_pr_verbose(bbts_[bbt_idx]->blks[blk_idx], fp);
				fprintf(fp, " ");
			}
			fprintf(fp, " ");
		}
		fprintf(fp, "\n");
	}
	if (fp != stdout) {
		fclose(fp);
	}
}


rocksdb::Status oc_block_manager::TEST_Pr_BBT()
{
	int i;
	struct nvm_addr lun_addr;
	struct nvm_ret ret;
	const struct nvm_bbt *ptr, *ptr2;
	char fname[64];
//	printf("BBTs: %p %p\n", bbts_, ssd_->dev_->bbts);


	for (i = 0; i < geo_->nluns; i++) {
		ptr = bbts_[_ch_lun2_bbt_idx(CH_THIS, i, geo_)];
		lun_addr.ppa = 0;
		lun_addr.g.lun = i;
//		printf("before: %p\n", ssd_->dev_->bbts[_ch_lun2_bbt_idx(CH_THIS, i, geo_)]);
//		if (ptr != (ptr2 = nvm_bbt_get(ssd_->dev_, lun_addr, &ret))) {
//			printf("Ptr!: %p, %p, %p\n", ptr, ssd_->dev_->bbts[_ch_lun2_bbt_idx(CH_THIS, i, geo_)],ptr2);
//		}
		sprintf(fname, "BBT_%d", i);
		TEST_My_nvm_bbt_pr(i, ptr, fname);
	}
	return rocksdb::Status::OK();

BBT_ERR:
	return rocksdb::Status::IOError("Get BBT", strerror(errno));
}

#define PRBBT_VERBOSE

void oc_block_manager::TEST_My_nvm_bbt_pr(int lun, const struct nvm_bbt *bbt, const char *fname)
{
	int nnotfree = 0;
	const int Pr_num = 4;
	int pred = 0, pr_sr = 0;
	if (fname && !(freopen(fname, "w", stdout))) {

	}
	if (!bbt) {
		printf("bbt { NULL }\n");
		return;
	}
	printf("LUN:%d", lun);
	printf("bbt {\n");
	printf("  addr");
	nvm_addr_pr(bbt->addr);
	printf("  nblks(%lu) {", bbt->nblks);
	for (int i = 0; i < bbt->nblks; i += bbt->dev->geo.nplanes) {
		int blk = i / bbt->dev->geo.nplanes;
#ifdef PRBBT_VERBOSE
		if (pred < Pr_num /*first Pr_num ones*/
			|| i == bbt->nblks - bbt->dev->geo.nplanes/*last one*/) {
#endif
			printf("\n    blk(%04d): [ ", blk);
			for (int blk = i; blk < (i + bbt->dev->geo.nplanes); ++blk) {
				my_nvm_bbt_state_pr(bbt->blks[blk]);
				printf(" ");
				if (bbt->blks[blk]) {
					++nnotfree;
				}
			}
			printf("]");
			pred++;
#ifdef PRBBT_VERBOSE
		} else if (!pr_sr) {
			printf("\n....");
			pr_sr = 1;
		}
#endif
	}
	printf("\n  }\n");
	printf("  #notfree(%d)\n", nnotfree);
	printf("}\n");
}

void oc_block_manager::def_ocblk_opt(struct Options *opt)
{
	opt->chunk_size = geo_->npages * geo_->page_nbytes; //block's size in bytes
	opt->policy = oc_options::kRoundRobin_Fixed;
	opt->bbt_cached = true;
}

//TODO - BBT_Management_Basic_Workflow: Get dev->bbts[...], Maintain, Flush

/*
 * Erase all blocks and set the corresponding BBT entry as free
 */
void oc_block_manager::InitClean()
{
	int i;
	struct nvm_bbt *ptr;
	//Erase
	for (i = 0; i < geo_->nluns; ++i) {
		//	printf("oc_block_manager::InitClean, Lun %d\n", i);
		s = oc_GC::EraseByLun(CH_THIS, i, this);
	}
	if (!s.ok()) {
		goto OUT;
	}

	//Set BBTs as all free.
	for (i = 0; i < bbts_length_; ++i) {
		ptr = bbts_[i];
		assert(ptr); //ptr should not be null.
		for (int j = 0; j < blks_length_; ++j) {
			ptr->blks[j] = BLK_UNUSED;
		}
	}

OUT:
	return;
}

/*
 * 
 */
void oc_block_manager::InitBBTs()
{
	int i;
	struct nvm_addr lun_addr;
	struct nvm_ret ret;
	const struct nvm_bbt *ptr;
	assert(opt_.bbt_cached); //now only support bbt cached.

	if (opt_.bbt_cached) {
		if (nvm_dev_set_bbts_cached(ssd_->dev_, 1)) {
			s = rocksdb::Status::IOError("oc_blk_mng: set bbt_cached", strerror(errno));
		}
	}

	if (s.ok()) {
		for (i = 0; i < geo_->nluns; i++) {
			lun_addr.ppa = 0;
			lun_addr.g.lun = i;
			ptr = nvm_bbt_get(ssd_->dev_, lun_addr, &ret);
			if (!ptr) {
				goto BBT_ERR;
			}
		}
		bbts_ = ssd_->dev_->bbts;
		bbts_length_ = geo_->nchannels * geo_->nluns; //sum of the LUNs.
		blks_length_ = geo_->nplanes * geo_->nblocks; //sum of the blks inside a LUNs.
	}
	return; //ok, then we can maintan the bbts now.

BBT_ERR:
	s = rocksdb::Status::IOError("oc_blk_mng: get bbts", strerror(errno));
}

void oc_block_manager::FlushBBTs() ///WARNING - Flush the BBT will cause it free the bbt entry.
{
	struct nvm_ret ret;
	if (nvm_bbt_flush_all(ssd_->dev_, &ret)) {
		s = rocksdb::Status::IOError("oc_blk_mng: flush bbts", strerror(errno));
	}
}


void oc_block_manager::Init()
{
	InitBBTs();
	InitClean();
}

oc_block_manager::oc_block_manager(oc_ssd *ssd, const struct nvm_geo *g) : ssd_(ssd), geo_(g)
{
	def_ocblk_opt(&opt_);
	Init();
}

/*
 * 
 */
rocksdb::Status oc_block_manager::New_oc_block_manager(oc_ssd *ssd, oc_block_manager **oc_blk_mng_ptr)
{
	oc_block_manager *ptr = new oc_block_manager(ssd, ssd->Geo());
	*oc_blk_mng_ptr = ptr->ok() ? ptr : NULL;
	return ptr->s;
}

} //namespace ocssd
} //namespace rocksdb
