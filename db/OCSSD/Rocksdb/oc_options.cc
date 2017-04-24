#include "oc_options.h"

namespace rocksdb {

    namespace ocssd {

        namespace oc_options {

//TODO - clean:
            const char *kDevPath = "/dev/nvme0n1";
            const char *kOCSSDMetaFileNameSuffix = ".ocssd";
            const ChunkingAlgor kChunkingAlgor = kRaid0;
            const GCPolicy kGCPolicy = kOneLunSerial;
            const AddrAllocPolicy kAddrAllocPolicy = kRoundRobin_Fixed;

            const int kOCFileNodeDegree = 6;
        };


    }//namespace ocssd
}//namespace rocksdb


