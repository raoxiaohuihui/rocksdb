//
// Created by raohui on 17-4-18.
//


#pragma once

#include <vector>

#include <stdint.h>
#include "rocksdb/slice.h"
#include "oc_page_cache.h"

namespace rocksdb {
    namespace ocssd {
        class BlockBuilder {
        public:
            BlockBuilder(const BlockBuilder &) = delete;

            void operator=(const BlockBuilder &) = delete;

            explicit BlockBuilder(int block_restart_interval,
                                  bool use_delta_encoding = true);

            // Reset the contents as if the BlockBuilder was just constructed.
            void Reset();

            // REQUIRES: Finish() has not been called since the last call to Reset().
            // REQUIRES: key is larger than any previously added key
            void Add(const Slice &key, const Slice &value);

            // Finish building the block and return a slice that refers to the
            // block contents.  The returned slice will remain valid for the
            // lifetime of this builder or until Reset() is called.
            Slice Finish();

            // Returns an estimate of the current (uncompressed) size of the block
            // we are building.
            inline size_t CurrentSizeEstimate() const { return estimate_; }

            // Returns an estimated block size after appending key and value.
            size_t EstimateSizeAfterKV(const Slice &key, const Slice &value) const;

            // Return true iff no entries have been added since the last Reset()
            bool empty() const {
                return buffer_->empty();
            }

        private:
            const int block_restart_interval_;
            const bool use_delta_encoding_;
            ocssd::oc_buffer *buffer_;    // Destination buffer
            std::vector<uint32_t> restarts_;  // Restart points
            size_t estimate_;
            int counter_;   // Number of entries emitted since restart
            bool finished_;  // Has Finish() been called?
            std::string last_key_;
        };

    }// namespace ocssd



} // namespace rocksdb

