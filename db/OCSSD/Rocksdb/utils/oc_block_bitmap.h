//
// Created by raohui on 17-6-1.
//

#ifndef ROCKSDB_OC_META_BITMAP_H
#define ROCKSDB_OC_META_BITMAP_H
#include "liblightnvm.h"
#include <bitset>
namespace rocksdb {
namespace ocssd {


typedef std::bitset<1020> Bitmap;
class oc_block_bitmap {
 public:
  oc_block_bitmap(struct nvm_geo const * geo);
  virtual ~oc_block_bitmap();
  size_t findFreedBlock(int index);
  void BlockBitmapUnset(int index1, int index2);
  void BlockBitmapSet(int index1, size_t index2);
  bool IsFull(int index);
  void printBitmap(int index);
  size_t *AllocationBlock(size_t blockNums);
 private:
  struct nvm_geo const * geo_;
  Bitmap *blockBitmap;
};
}}
#endif //ROCKSDB_OC_META_BITMAP_H
