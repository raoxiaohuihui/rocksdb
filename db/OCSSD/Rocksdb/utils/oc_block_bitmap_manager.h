//
// Created by raohui on 17-6-3.
//

#ifndef ROCKSDB_OC_BLOCK_BITMAP_MANAGER_H
#define ROCKSDB_OC_BLOCK_BITMAP_MANAGER_H
#include "oc_block_bitmap.h"
#include "oc_tree.h"
namespace rocksdb {
namespace ocssd {

class oc_block_bitmap_manager {
 private:
  oc_block_bitmap *lunManager ;
  addr::blk_addr_handle blk_addr_handle;
  struct nvm_geo const * geo_;
  struct tree_meta const *tm_;

 public:

  oc_block_bitmap_manager(const nvm_geo *geo_, const tree_meta *tm_);
  virtual ~oc_block_bitmap_manager();
  addr::blk_addr *allocateBlocksForFile(size_t FileSize);
};
}
}
#endif //ROCKSDB_OC_BLOCK_BITMAP_MANAGER_H
