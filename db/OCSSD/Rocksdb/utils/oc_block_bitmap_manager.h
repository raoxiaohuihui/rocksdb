//
// Created by raohui on 17-6-3.
//

#ifndef ROCKSDB_OC_BLOCK_BITMAP_MANAGER_H
#define ROCKSDB_OC_BLOCK_BITMAP_MANAGER_H
#include <list>
#include "oc_block_bitmap.h"
#include "oc_tree.h"
namespace rocksdb {
namespace ocssd {

class oc_block_bitmap_manager {
 private:
  oc_block_bitmap *blockBitMapManager ;
  addr::blk_addr_handle blk_addr_handle;
  struct nvm_geo const * geo_;
  struct tree_meta const *tm_;

 public:

  oc_block_bitmap_manager(const nvm_geo *geo, const tree_meta *tm_);
  virtual ~oc_block_bitmap_manager();
  const addr::blk_addr_handle &getBlk_addr_handle() const;
  void setBlk_addr_handle(const addr::blk_addr_handle &blk_addr_handle);
  const nvm_geo *getGeo_() const;
  void setGeo_(const nvm_geo *geo_);
  const tree_meta *getTm_() const;
  void setTm_(const tree_meta *tm_);
  std::__cxx11::list<addr::blk_addr> allocateBlocksForFile(size_t FileSize);
};
}
}
#endif //ROCKSDB_OC_BLOCK_BITMAP_MANAGER_H
