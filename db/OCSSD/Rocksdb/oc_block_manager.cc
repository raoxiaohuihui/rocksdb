#include "oc_ssd.h"
#include "oc_block_manager.h"

#include "util/mutexlock.h"

#include <time.h>
#include <cassert>
#include <set>

namespace rocksdb {
    namespace ocssd {

        namespace {

            const oc_block_manager::BlkState_t BLK_UNUSED = NVM_BBT_FREE;
            const oc_block_manager::BlkState_t BLK_USED = NVM_BBT_HMRK;
            const oc_block_manager::BlkState_t BLK_INVALID = NVM_BBT_GBAD;
            const int CH_THIS = 0;    //Qemu OCSSD is only 1 channel
/* 
 * The flags transition 
 *  
 *             write_opr                 update_opr/remove_opr
 * BLK_UNUSED --------------> BLK_USED --------------------->BLK_INVALID
 *     |                                                       |
 *     |<------------------------------------------------------|
 *     				          erase_opr
 */
        };

/* 
 * BBTs Layout:
 *  		  L0    L2   L3   L4
 *  		  p0p1  p0p1 p0p1 p0p1	    
 * B0   =     = =   = =  = =  = =
 * B1   =     = =   = =  = =  = =
 * B2   =     = =   = =  = =  = =
 * B3   =     = =   = =  = =  = =
 * ......
 * Bn   =     = =   = =  = =  = = 
 * 			  BBT0  BBT1 BBT2 BBT3 
 */

        static inline int _ch_lun2_bbt_idx(int ch, int lun, const struct nvm_geo *geo) {
            return ch * geo->nluns + lun;
        }

        static inline int _pl_blk2_bbtblks_idx(int pl, int blk, const struct nvm_geo *geo) {
            return blk * geo->nplanes + pl;
        }

        static void my_nvm_bbt_state_pr(int state) {
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

        static void my_nvm_bbt_state_pr_verbose(int state, FILE *fp = stdout) {
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

        static void gettimestr(std::string &str) {
            char buf[32];
            time_t timep;
            struct tm *p;
            int ret;
            time(&timep);
            p = gmtime(&timep);
            str.clear();
            ret = snprintf(buf, 32, "%d/%d/%d", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday);
            str.append(buf);
            ret = snprintf(buf, 32, " %d:%d:%d", p->tm_hour, p->tm_min, p->tm_sec);
            str.append(buf);
        }


//REQUIRE: Lun and Pl's value should be less then 16-length
#define MakeLunAndPlane(__LUN, __PL) ({ \
            uint32_t __l = __LUN;       \
            uint32_t __p = __PL;        \
            uint32_t __lap = (__l << 16) | __p; \
            __lap; \
            })

#define GetLun(__LAP) ({ uint32_t __mask = ((uint32_t)1 << 16) - 1; \
            (__LAP & (__mask << 16)) >> 16; \
            })

#define GetPlane(__LAP) ({ uint32_t __mask = ((uint32_t)1 << 16) - 1; \
            __LAP & __mask; \
            })

        static inline size_t _get_left_blks(oc_block_manager::LunAndPlane_t lap, const struct nvm_geo *geo) {
            return ((geo->nluns - GetLun(lap)) * geo->nplanes) - GetPlane(lap);
        }

        void oc_block_manager::TEST_Lap() {
            for (int i; i < geo_->nluns; i++) {
                for (int j = 0; j < geo_->nplanes; j++) {
                    printf("<L%dP%d> %zu\n", i, j, _get_left_blks(MakeLunAndPlane(i, j), geo_));
                }
            }
        }

        void oc_block_manager::TEST_Pr_UM() {
            TEST_RR_Addr_Pr(next);
        }

        void oc_block_manager::TEST_RR_Addr_Pr(struct rr_addr x) {
            printf("Blk%u,<L%uP%u>", x.block, GetLun(x.lap), GetPlane(x.lap));
        }

        void oc_block_manager::TEST_Itr_rr_addr() {
            rr_addr st, ed;
            st.lap = MakeLunAndPlane(0, 1);
            st.block = 1;

            ed.lap = MakeLunAndPlane(2, 0);
            ed.block = 3;

            Itr_rr_addr itr(st, ed, geo_);
            itr.TEST_Iteration();
        }


        bool oc_block_manager::rr_addr::ok(const struct nvm_geo *limit) {
            bool ret = true;
            uint32_t l = GetLun(this->lap);
            uint32_t p = GetPlane(this->lap);
            if (l >= limit->nluns || p >= limit->nplanes || this->block >= limit->nblocks) {
                ret = false;
            }
            return ret;
        }


        oc_block_manager::rr_addr &oc_block_manager::rr_addr::Increment(const struct nvm_geo *limit) {
            uint32_t l = GetLun(this->lap), p = GetPlane(this->lap);
            p++;
            if (p >= limit->nplanes) {
                l++;
                p = 0;
            }
            if (l >= limit->nluns) {
                this->block++;
                l = 0;
            }
            this->lap = MakeLunAndPlane(l, p);
            return *this;
        }

        oc_block_manager::rr_addr &oc_block_manager::rr_addr::Increment(size_t blks, const struct nvm_geo *limit) {
            int itr = 0;
            uint32_t l, p;
            size_t left = _get_left_blks(this->lap, limit);
//	printf("\n %zu\n", left);
            while (1) {
                if (blks < left) {
                    l = GetLun(this->lap) + (blks / limit->nplanes);
                    p = GetPlane(this->lap) + (blks % limit->nplanes);
                    while (p >= limit->nplanes) {
                        p -= limit->nplanes;
                        l++;
                    }
//			printf(" %u, %u\n", l, p);
                    this->lap = MakeLunAndPlane(l, p);
                    goto OUT;
                } else {
                    assert(itr == 0); // at most go 1 time here.
                    this->lap = 0;
                    this->block++;
                    blks -= left;
                    if (blks > 0) {
                        this->block = this->block + (blks / (limit->nluns * limit->nplanes));
                        left = limit->nluns * limit->nplanes;
                        blks = blks % (limit->nluns * limit->nplanes);
                    }
                    //a more round.
                }
                itr++;
            }
            OUT:
            return *this;
        }

        size_t oc_block_manager::rr_addr::Minus(const rr_addr &rhs, const struct nvm_geo *limit) {
            size_t blks, blks_mi;
            uint32_t l1 = GetLun(this->lap), p1 = GetPlane(this->lap),
                    l2 = GetLun(rhs.lap), p2 = GetPlane(rhs.lap);
            if ((*this) <= rhs) {
                return 0;
            }

            blks = (this->block - rhs.block) * (limit->nluns * limit->nplanes);
            blks += l1 * limit->nplanes;
            blks += p1;

            blks_mi = l2 * limit->nplanes;
            blks_mi += p2;

            return blks - blks_mi;
        }


        bool oc_block_manager::rr_addr::operator<=(const rr_addr &rhs) {
            uint32_t l1 = GetLun(this->lap), p1 = GetPlane(this->lap),
                    l2 = GetLun(rhs.lap), p2 = GetPlane(rhs.lap);
            if (this->block != rhs.block) {
                return this->block < rhs.block;
            } else if (l1 != l2) {
                return l1 < l2;
            } else if (p1 != p2) {
                return p1 < p2;
            } else {
                return true;
            }
        }

        oc_block_manager::rr_addr &oc_block_manager::rr_addr::operator=(const rr_addr &rhs) {
            this->lap = rhs.lap;
            this->block = rhs.block;
            return *this;
        }


        void oc_block_manager::TEST_Add() {
            size_t testcase[] = {6};
            rr_addr r0, r1;
            r0.lap = MakeLunAndPlane(3, 1);
            r0.block = 0;
            r1 = next;
            for (int i = 0; i < sizeof(testcase) / sizeof(testcase[0]); ++i) {
                next = r0;
                TEST_Pr_UM();
                printf("+ %2zu = ", testcase[i]);
                Add_blks(testcase[i]);
                TEST_Pr_UM();
                printf("\n");
            }
            next = r1;
        }

        void oc_block_manager::Add_blks(size_t blks)///TODO - [Block - Lun - Plane] addressing need to be refine!!!
        {
            next.Increment(blks, geo_);
        }

        void oc_block_manager::Itr_rr_addr::SetBBTInCache(struct nvm_bbt **bbts, BlkState_t flag) {
            uint32_t l, p;
            while (blks) {
                l = GetLun(st.lap);
                p = GetPlane(st.lap);

                bbts[_ch_lun2_bbt_idx(CH_THIS, l, limit)]->blks[_pl_blk2_bbtblks_idx(p, st.block, limit)] = flag;

                st.Increment(limit);
                blks--;
            }
        }

        void oc_block_manager::Itr_rr_addr::TEST_Iteration() {
            uint32_t l, p;
            int i = 1;
            while (blks) {

                oc_block_manager::TEST_RR_Addr_Pr(st);
                printf(" -> ");
                if (0 == i % 5) {
                    printf("\n");
                }
                st.Increment(limit);
                blks--;
                i++;
            }
            printf("\n");
        }


        rocksdb::Status oc_block_manager::Itr_rr_addr::Write() {
            rocksdb::Status s;
            return s;
        }

        rocksdb::Status oc_block_manager::Itr_rr_addr::Read() {
            rocksdb::Status s;
            return s;
        }


/*
 * Iterate on [st, ed) and set the corresponding slot in bbt.
 */
        void oc_block_manager::Set_stripe_blks_as(struct StripeDes des, BlkState_t flag) {
            {
                rocksdb::MutexLock l(&bbts_lock);
                Itr_rr_addr itr(des.st, des.ed, geo_);
                itr.SetBBTInCache(bbts_, flag);
            }
        }


        rocksdb::Status oc_block_manager::AllocStripe(size_t bytes, StripeDes *sd) {
            assert(bytes % opt_.chunk_size == 0);
            rocksdb::Status s;
            size_t blks = bytes / opt_.chunk_size;
            sd->st = next;
            {
                rocksdb::MutexLock l(&next_lock);
                Add_blks(blks);
            }
            sd->ed = next;

            Set_stripe_blks_as(*sd, BLK_USED);

            return s;
        }

        rocksdb::Status oc_block_manager::FreeStripe(StripeDes *sd) {
            rocksdb::Status s;

            Set_stripe_blks_as(*sd, BLK_INVALID);

            return s;
        }

        rocksdb::Status oc_block_manager::FreeStripeArray(StripeDes *sds, int num) {
            rocksdb::Status s;

            for (int i = 0; i < num; i++) {
                Set_stripe_blks_as(sds[i], BLK_INVALID);
            }

            return s;
        }

        bool oc_block_manager::ok() {
            return s.ok();
        }

        void oc_block_manager::TEST_Pr_Opt_Meta() {
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
        void oc_block_manager::Pr_BlocksState(const char *fname) {
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


        rocksdb::Status oc_block_manager::TEST_Pr_BBT() {
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

        void oc_block_manager::TEST_My_nvm_bbt_pr(int lun, const struct nvm_bbt *bbt, const char *fname) {
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

        void oc_block_manager::def_ocblk_opt(struct Options *opt) {
            opt->chunk_size = geo_->npages * geo_->page_nbytes; //block's size in bytes
            opt->policy = oc_options::kRoundRobin_Fixed;
            opt->bbt_cached = true;
        }

//TODO - BBT_Management_Basic_Workflow: Get dev->bbts[...], Maintain, Flush

/*
 * Erase all blocks and set the corresponding BBT entry as free
 */
        void oc_block_manager::InitClean() {
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
                assert(ptr);//ptr should not be null.
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
        void oc_block_manager::InitBBTs() {
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


        void oc_block_manager::Init() {
            InitBBTs();
            InitClean();
        }

        oc_block_manager::oc_block_manager(oc_ssd *ssd, const struct nvm_geo *g) : ssd_(ssd), geo_(g) {
            def_ocblk_opt(&opt_);
            Init();
        }

/*
 * 
 */
        rocksdb::Status oc_block_manager::New_oc_block_manager(oc_ssd *ssd, oc_block_manager **oc_blk_mng_ptr) {
            oc_block_manager *ptr = new oc_block_manager(ssd, ssd->Geo());
            *oc_blk_mng_ptr = ptr->ok() ? ptr : NULL;
            return ptr->s;
        }

    } //namespace ocssd
} //namespace rocksdb
