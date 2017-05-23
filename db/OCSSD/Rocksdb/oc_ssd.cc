//OCSSD-Optimization Modules headers
#include "oc_ssd.h"
#include "oc_block_manager.h"
#include "oc_page_cache.h"


#include "util/coding.h"
#include "util/mutexlock.h"

namespace rocksdb {
namespace ocssd {

oc_ssd::oc_ssd() 
try : des_(new oc_ssd_descriptor()), 
//TODO: refine the blkmng_ & pgp_ as using exception
//	blkmng_(new oc_block_manager()), 
//	pgp_(new )
{

} catch (const dev_init_exception & e){ // exception throwed by des_
	throw e;
} catch (const std::exception & e){ //other exception
	throw e;
}

oc_ssd::~oc_ssd()
{
}


void oc_ssd::EncodeTo(struct oc_ssd_descriptor *ocdes, char *buf)
{
	std::string str;
	PutLengthPrefixedSlice(&str, ocdes->dev_path_);
	size_t num = ocdes->files_.size();
}
void oc_ssd::DecodeFrom(struct oc_ssd_descriptor *ocdes, char *buf)
{

}


} //namespace ocssd
} //namespace rocksdb
