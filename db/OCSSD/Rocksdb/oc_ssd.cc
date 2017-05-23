//OCSSD-Optimization Modules headers
#include "oc_ssd.h"
#include "oc_block_manager.h"
#include "oc_page_cache.h"


#include "util/coding.h"
#include "util/mutexlock.h"

namespace rocksdb {
namespace ocssd {

oc_ssd::oc_ssd() 
: des_(new oc_ssd_descriptor(oc_options::kDevPath)), 
dev_(NULL),
geo_(NULL),
blkmng_(NULL),
pgp_(NULL)
{
	init();
	if (s.ok()) {
		s = oc_block_manager::New_oc_block_manager(this, &blkmng_);
	}
	if (s.ok()) {
		s = oc_page_pool::New_page_pool(this->geo_, &page_pool_); 
	}
}
oc_ssd::~oc_ssd()
{
	Cleanup();
}

void oc_ssd::init()
{
	//Open device
	dev_ = nvm_dev_open(des_->dev_path_.c_str());
	if (!dev_) {
		s = Status::IOError("OCSSD Setup device", strerror(errno));
		return;
	}
	//geo
	geo_ = nvm_dev_get_geo(dev_);
	if (!geo_) {
		s = Status::IOError("OCSSD Setup geometry", strerror(errno));
		return;
	}

	//TODO: pmode issue - check liblightnvm version
}
void oc_ssd::Cleanup()
{
	nvm_dev_close(dev_);

	delete page_pool_;

	delete des_;
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


oc_file* oc_ssd::TEST_New_file(const char *fname)
{
	oc_file *ptr;
    rocksdb::Status s = oc_file::New_oc_file(this, fname, &ptr);
	return ptr;
}


} //namespace ocssd
} //namespace rocksdb
