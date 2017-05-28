#include "oc_ssd.h"
#include "oc_page_cache.h"

int oc_buffer_testx()
{
	rocksdb::ocssd::oc_ssd ssd;
	rocksdb::ocssd::oc_page_pool *pool;
	
	pool = ssd.Get_pgpool();
	rocksdb::ocssd::oc_buffer buff(pool);

	buff.TEST_WritableFile_Clear_Again(); 

	return 0;
}


int oc_buffer_writablefile_test()
{
	rocksdb::ocssd::oc_ssd ssd;
	rocksdb::ocssd::oc_page_pool *pool;
	

	pool = ssd.Get_pgpool();
	rocksdb::ocssd::oc_buffer buff(pool);

	buff.TEST_WritableFile(); 

	return 0;
}

int oc_buffer_test()
{
	rocksdb::ocssd::oc_ssd ssd;
	rocksdb::ocssd::oc_page_pool *pool;
	

	pool = ssd.Get_pgpool();
	rocksdb::ocssd::oc_buffer buff(pool);

	buff.TEST_Large2(); 

	return 0;
}




int page_pool_test()
{
	rocksdb::ocssd::oc_ssd ssd;
	rocksdb::ocssd::oc_page_pool *pool;
	pool = ssd.Get_pgpool();
	pool->TEST_Basic();

	return 0;
}

int main()
{
	oc_buffer_testx();
	return 0;
}
