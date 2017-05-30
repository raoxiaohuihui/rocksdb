#ifndef YWJ_OCSSD_OC_TREE_H
#define YWJ_OCSSD_OC_TREE_H

//liblightnvm headers
#include "liblightnvm.h"
#include "nvm.h"

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

struct blk_addr_handle{ // a handle should attach to a tree.

};

struct config{
	const struct nvm_geo * geo_;
	struct tree_meta * tm_;
	struct blk_addr_handle * bah_;
	int tree_num_;
};

struct config *kConfig;

void addr_init(class oc_ssd *ssd);
void addr_release();

} // namespace addr

/*Tree's logic goes here*/




} // namespace ocssd
} // namespace rocksdb
#endif
