//
// Created by raohui on 17-6-3.
//

#include "oc_block_bitmap_manager.h"
namespace rocksdb {
namespace ocssd {

addr::blk_addr *oc_block_bitmap_manager::allocateBlocksForFile(size_t FileSize) {
  size_t blockNums = FileSize / geo_->nluns / (geo_->npages * geo_->page_nbytes);
  size_t blocksAndPlane[blockNums + 1];
  addr::blk_addr blk_addrs[geo_->nluns * blockNums];
  for (int i = 0; i < geo_->nluns; ++i) {
    blocksAndPlane = lunManager->AllocationBlock(blockNums);
    size_t plane = blocksAndPlane[blockNums];
    for (int j = 0; j < blockNums; ++j) {
      blk_addr_handle.MakeBlkAddr(0, i, plane, j, blk_addrs + j + i * blockNums);
    }
  }
  return blk_addrs;
}
oc_block_bitmap_manager::oc_block_bitmap_manager(const nvm_geo *geo_, const oc_block_bitmap_manager::tree_meta *tm_)
    : geo_(geo_), tm_(tm_), blk_addr_handle(geo_, tm_) {
  lunManager = new oc_block_bitmap[geo_->nluns];
}
oc_block_bitmap_manager::~oc_block_bitmap_manager() {
  delete geo_;
  delete tm_;
  delete[] lunManager;
}
}
}