#ifndef YWJ_OCSSD_OC_OPTIONS_H
#define YWJ_OCSSD_OC_OPTIONS_H
namespace rocksdb {
namespace ocssd {

namespace oc_options{

typedef enum {
	kRaid0 = 0x00,   //No parity ,so Reconstruction_Read is not supported.
	kRaid5 = 0x01    //with parity spread around the stripe.
}ChunkingAlgor;
typedef enum {
	kOneLunSerial = 0x00,    //Erase Operation is emited to one LUN by serializtion
	kAllIn = 0x01
}GCPolicy;
typedef enum {
	kRoundRobin_Fixed = 0x00,
	kRoundRobin_Flexible = 0x01
}AddrAllocPolicy;

extern const char *kDevPath;
extern const char *kOCSSDMetaFileNameSuffix;
extern const ChunkingAlgor kChunkingAlgor;
extern const GCPolicy kGCPolicy;
extern const AddrAllocPolicy kAddrAllocPolicy;
extern const int kOCFileNodeDegree;
}

}//namespace ocssd
}//namespace rocksdb



#endif
