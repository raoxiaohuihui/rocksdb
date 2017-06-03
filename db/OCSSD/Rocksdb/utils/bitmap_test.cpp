//
// Created by raohui on 17-6-1.
//
#include "oc_block_bitmap_manager.h"
#include <iostream>
int main() {
  rocksdb::ocssd::addr::tree_meta const *tm_ = new rocksdb::ocssd::addr::tree_meta;
  struct nvm_geo *geo = new struct nvm_geo;
  geo->nluns = 4;
  geo->page_nbytes = 1024;
  geo->npages = 2;
  geo->nplanes = 2;

  rocksdb::ocssd::oc_block_bitmap_manager oc_block_bitmap_manager(geo, tm_);
  std::list<rocksdb::ocssd::addr::blk_addr> blkAddrList = oc_block_bitmap_manager.allocateBlocksForFile(8192);
  rocksdb::ocssd::addr::blk_addr_handle blk_addr_handle = oc_block_bitmap_manager.getBlk_addr_handle();
  for (std::list<rocksdb::ocssd::addr::blk_addr>::iterator i = blkAddrList.begin(); i != blkAddrList.end(); i++) {
    rocksdb::ocssd::addr::blk_addr blk_addr = *i;
    blk_addr_handle.PrBlkAddr(&blk_addr, false, "##");
  }

}