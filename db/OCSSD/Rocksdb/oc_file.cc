//Dependencies headers
#include "oc_ssd.h"
#include "oc_block_manager.h"


#include "oc_file.h"

#include <malloc.h>

namespace rocksdb {

namespace ocssd {


oc_file::oc_file()
{
}

oc_file::~oc_file()
{
}

rocksdb::Status oc_file::Append()
{
	return rocksdb::Status();
}

rocksdb::Status oc_file::RandomReadByOffset()
{
	return rocksdb::Status();
}

rocksdb::Status oc_file::Flush()
{
	return rocksdb::Status();
}


} //namespace ocssd
} //namespcae rocksdb
