//
// Created by raohui on 17-6-3.
//

#include "oc_block_bitmap_manager.h"
namespace rocksdb {
namespace ocssd {

std::list<addr::blk_addr> oc_block_bitmap_manager::allocateBlocksForFile(size_t FileSize) {
  size_t blockNums = FileSize / geo_->nluns / (geo_->npages * geo_->page_nbytes);
  size_t blocksAndPlane[blockNums + 1];
  std::list<addr::blk_addr> blkAddrList;
  addr::blk_addr *blk_addrs = new addr::blk_addr[geo_->nluns * blockNums];
  for (int i = 0; i < geo_->nluns; ++i) {
    blocksAndPlane = blockBitMapManager->AllocationBlock(blockNums);
    size_t plane = blocksAndPlane[blockNums];
    for (int j = 0; j < blockNums; ++j) {
      addr::blk_addr *blk_addr = new addr::blk_addr;
      blk_addr_handle.MakeBlkAddr(0, i, plane, j, blk_addr);
      blkAddrList.push_back(*blk_addr);
    }
  }
  return blkAddrList;
}
oc_block_bitmap_manager::oc_block_bitmap_manager(const nvm_geo *geo, const addr::tree_meta *tm_)
    : geo_(geo), tm_(tm_), blk_addr_handle(geo, tm_) {
  blockBitMapManager = new oc_block_bitmap[geo_->nluns];
  for (int i = 0; i < geo_->nluns; ++i) {
    blockBitMapManager[i].setGeo_(geo_);
    blockBitMapManager[i].setBlockBitmap(new Bitmap[geo_->nplanes]);
  }
}
oc_block_bitmap_manager::~oc_block_bitmap_manager() {
  delete geo_;
  delete tm_;
  delete[] blockBitMapManager;
}
const addr::blk_addr_handle &oc_block_bitmap_manager::getBlk_addr_handle() const {
  return blk_addr_handle;
}
void oc_block_bitmap_manager::setBlk_addr_handle(const addr::blk_addr_handle &blk_addr_handle) {
  oc_block_bitmap_manager::blk_addr_handle = blk_addr_handle;
}
const nvm_geo *oc_block_bitmap_manager::getGeo_() const {
  return geo_;
}
void oc_block_bitmap_manager::setGeo_(const nvm_geo *geo_) {
  oc_block_bitmap_manager::geo_ = geo_;
}
const oc_block_bitmap_manager::tree_meta *oc_block_bitmap_manager::getTm_() const {
  return tm_;
}
void oc_block_bitmap_manager::setTm_(const oc_block_bitmap_manager::tree_meta *tm_) {
  oc_block_bitmap_manager::tm_ = tm_;
}

}
}