//
// Created by raohui on 17-6-1.
//

#include <iostream>
#include "oc_block_bitmap.h"
#include ""
namespace rocksdb {
namespace ocssd {

oc_block_bitmap::oc_block_bitmap(struct nvm_geo const *geo) : geo_(geo) {
  blockBitmap = new Bitmap[geo->nplanes];

}
oc_block_bitmap::~oc_block_bitmap() {
  delete[] blockBitmap;
}
size_t oc_block_bitmap::findFreedBlock(int index) {
  return blockBitmap[index].flip()._Find_next(0);
}
void oc_block_bitmap::BlockBitmapUnset(int index1, int index2) {
  blockBitmap[index1][index2] = 0;
}
void oc_block_bitmap::BlockBitmapSet(int index1, size_t index2) {
  blockBitmap[index1][index2] = 1;
}
bool oc_block_bitmap::IsFull(int index) {
  return blockBitmap[index].flip() == 0;
}
void oc_block_bitmap::printBitmap(int index) {
  std::cout << blockBitmap[index].to_string() << std::endl;
}
size_t *oc_block_bitmap::AllocationBlock(size_t blockNums) {
  size_t blocksAndPlane[blockNums + 1];
  int i = 0;
  for (; i < geo_->nplanes; ++i) {
    if (!IsFull(i)) {
      break;
    }
  }
  for (int j = 0; j < blockNums; ++j) {
    blocksAndPlane[j] = findFreedBlock(i);
    BlockBitmapSet(i, blocksAndPlane[j]);
  }
  blocksAndPlane[blockNums] = (size_t) i;
  return blocksAndPlane;
}
}
}

