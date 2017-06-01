#ifndef YWJ_OCSSD_OC_TREE_H
#define YWJ_OCSSD_OC_TREE_H

//liblightnvm headers
#include "liblightnvm.h"
#include "../nvm.h"

#include <stdint.h>

namespace rocksdb {
namespace ocssd {


class oc_ssd;

namespace addr{

//1 channel 1 ext_tree
//for qemu:
// @ch is fixed to 0
// @stlun & @nluns is changable
// a channel contains multi ext_tree
//
//for real-device:
// @ch is changable
// @stlun & @nluns is fixed
// this ext_tree's lun range: [stlun, stlun + nluns - 1]
struct tree_meta {
    //fields for address-format 
    size_t ch;
    size_t stlun;
    size_t nluns;

    //other fields
};


/*
 * Block Address Format: >>>>>>>>default
 *
 *        |   ch   |  blk   |  pl   |   lun   |
 * length   _l[0]    _l[1]    _l[2]    _l[3]
 * idx      0        1        2        3
 */

/* 
 * Block Address Layout:(format 3)
 * 				 
 *  			 ext_tree_0						ext_tree_1					....
 * 			  |-----ch_0---------|			|-----ch_1---------|			....
 *  		  L0    L2    L3    L4			L0    L2    L3    L4	  
 * B0  p0     =     =     =     =		    =     =     =     =				Low ----> High
 *     p1     =     =     =     =           =     =     =     =				|   ---->
 * B1  p0     =     =     =     =		    =     =     =     =				|
 *     p1     =     =     =     =           =     =     =     =				|
 * B2  p0     =     =     =     =		    =     =     =     =				High
 *     p1     =     =     =     =           =     =     =     = 
 * B3  p0     =     =     =     =		    =     =     =     =
 *     p1     =     =     =     =           =     =     =     =
 * Bn  p0     =     =     =     =		    =     =     =     =
 *     p1     =     =     =     =           =     =     =     =
 * 			  BBT0  BBT1  BBT2  BBT3 		BBT0  BBT1  BBT2  BBT3
 */

struct blk_addr{
	uint64_t __buf;
};

class blk_addr_handle{ // a handle should attach to a tree.
public:
	blk_addr_handle(struct nvm_geo const * g, struct tree_meta const * tm);
	~blk_addr_handle();

	enum{
		RetOK	 = 0,
		FieldCh  = 1,
		FieldLun = 2,
		FieldPl  = 3,
		FieldBlk = 4,
		FieldOOR = 5,		//Out of range

		AddrInvalid = 6,
		CalcOF = 7			//calculation: overflow/underflow occur
	};
	
	int MakeBlkAddr(size_t ch, size_t lun, size_t pl, size_t blk, struct blk_addr* addr);
	ssize_t GetFieldFromBlkAddr(struct blk_addr const * addr, int field, bool isidx);
	ssize_t SetFieldBlkAddr(size_t val, int field, struct blk_addr * addr, bool isidx);

	/*
	 * @blk_addr + @x 
	 * warning - @blk_addr will be change 
	 */
	void BlkAddrAdd(size_t x, struct blk_addr* addr);
	
	/*
	 * @blk_addr - @x 
	 * warning - @blk_addr will be change  
	 */
	void BlkAddrSub(size_t x, struct blk_addr* addr);
	
	/*
	 * return -1 if @lhs < @rhs; 
	 * 		  0  if @lhs == @rhs;
	 * 		  1  if @lhs > @rhs; 
	 */
	int BlkAddrCmp(const struct blk_addr* lhs, const struct blk_addr* rhs);

	bool CalcOK();
	
    /*
     * return @addr - @_lowest, if mode == 0; 
     * return @_highest - @addr, if mode == 1;
     */
    size_t BlkAddrDiff(const struct blk_addr* addr, int mode);

	/*
	 * valid value range: [0, field_limit_val).
	 * @ret - 0 - OK. 
	 *      - otherwise return value means corresponding field not valid.
	 */
	int BlkAddrValid(size_t ch, size_t lun, size_t pl, size_t blk);




	void PrInfo();
	void PrBlkAddr(struct blk_addr* addr, bool pr_title, const char *prefix);
	void PrTitleMask(struct blk_addr* addr);

private:

	struct AddrFormat {
		int ch;
		int lun;
		int pl;
		int blk;
		AddrFormat()
			: ch(0), lun(3), pl(2), blk(1)
		{
		}
	};

	void init();
	void do_sub_or_add(size_t x, struct blk_addr* addr, int mode);
	void do_sub(size_t* aos, size_t* v, struct blk_addr *addr);
	void do_add(size_t* aos, size_t* v, struct blk_addr *addr);


	struct nvm_geo const * geo_;
	struct tree_meta const *tm_; 
	int status_;						//status of last operation. 
	AddrFormat format_;

	struct blk_addr lowest;
	struct blk_addr highest;

	int lmov_[4];
	uint64_t mask_[4];
	size_t usize_[4];
};

struct config{
	const struct nvm_geo * geo_;
	struct blk_addr_handle * bah_;
	int tree_num_;
};

void addr_init(class oc_ssd *ssd);
void addr_release();

} // namespace addr

/*Tree's logic goes here*/
struct extent{
	uint64_t addr_st_buf;
	uint64_t addr_ed_buf;
	uint32_t free_bitmap;
	uint32_t junk_bitmap;
}__attribute__((aligned(8)));		//24B

struct meta_obj{
	uint32_t free_vblk_num;
	uint16_t obj_id;
	uint16_t reserve;
}__attribute__((aligned(8)));		//8B


#define Leaf_Node_Degree 31
#define Non_Leaf_Node_Degree 35
#define Non_Leaf_Node_Degree_Meta 105 //MUST be {3 * NONLeaf_Node_Degree}

struct leaf_node{
	uint16_t el_2_mobj_num; 	//ext_len_2_meta_obj_num
	uint16_t el_8_mobj_num; 
	uint16_t el_4_mobj_num;
	uint16_t el_none_mobj_num;

	uint32_t free_vblk_sum_2;	//free_vblock_number of extent_len = 2
	uint32_t free_vblk_sum_4;
	uint32_t free_vblk_sum_8;
	uint32_t free_vblk_sum_none;

	struct meta_obj mobjs[Leaf_Node_Degree];
	struct extent exts[Leaf_Node_Degree];
	
	uint32_t reserve3[1];
	uint32_t node_type;			//0
}__attribute__((aligned(8)));	//1024B

struct non_leaf_node{
	uint16_t el_2_mobj_num; 	//ext_len_2_meta_obj_num
	uint16_t el_8_mobj_num; 
	uint16_t el_4_mobj_num;
	uint16_t el_none_mobj_num;

	uint32_t free_vblk_sum_2;	//free_vblock_number of extent_len = 2
	uint32_t free_vblk_sum_4;
	uint32_t free_vblk_sum_8;
	uint32_t free_vblk_sum_none;

	struct meta_obj mobjs[Non_Leaf_Node_Degree_Meta];
	uint32_t node_ids[Non_Leaf_Node_Degree];
	
	uint32_t reserve3[3];
	uint32_t node_type;			//1
}__attribute__((aligned(8)));	//1024B





} // namespace ocssd
} // namespace rocksdb
#endif
