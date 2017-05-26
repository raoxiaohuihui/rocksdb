//OCSSD-Optimization Modules headers
#include "oc_ssd.h"
#include "oc_block_manager.h"
#include "oc_page_cache.h"


#include "util/coding.h"
#include "util/mutexlock.h"

namespace rocksdb {
namespace ocssd {

oc_ssd::oc_ssd() throw (oc_excpetion)
try : des_(new oc_ssd_descriptor()), 
//TODO: refine the blkmng_ & pgp_ as using exception
blkmng_(new oc_block_manager(this)) 
//	pgp_(new )
{

} catch (const dev_init_exception & e){ 
	throw e;
} catch (const bbt_operation_not_good & e){
	delete des_;
	throw e;
} catch (const std::exception& e) { 
	throw e;
}

oc_ssd::~oc_ssd()
{
	delete des_;
	delete blkmng_;
}


} //namespace ocssd
} //namespace rocksdb
