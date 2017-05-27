#include "oc_ssd.h"
#include "oc_page_cache.h"

int oc_buffer_testx()
{
	rocksdb::ocssd::oc_ssd ssd;
	rocksdb::ocssd::oc_page_pool *pool;
	
	if (ssd.ok()) {
		printf("construct ok.\n");
	}else{
		printf("ocssd construct failed: %s.\n", ssd.s.ToString().c_str()); 
	}
	pool = ssd.PagePool();
	rocksdb::ocssd::oc_buffer buff(pool);

	buff.TEST_WritableFile_Clear_Again(); 

	return 0;
}


int oc_buffer_writablefile_test()
{
	rocksdb::ocssd::oc_ssd ssd;
	rocksdb::ocssd::oc_page_pool *pool;
	
	if (ssd.ok()) {
		printf("construct ok.\n");
	}else{
		printf("ocssd construct failed: %s.\n", ssd.s.ToString().c_str()); 
	}
	pool = ssd.PagePool();
	rocksdb::ocssd::oc_buffer buff(pool);

	buff.TEST_WritableFile(); 

	return 0;
}

int oc_buffer_test()
{
	rocksdb::ocssd::oc_ssd ssd;
	rocksdb::ocssd::oc_page_pool *pool;
	
	if (ssd.ok()) {
		printf("construct ok.\n");
	}else{
		printf("ocssd construct failed: %s.\n", ssd.s.ToString().c_str()); 
	}
	pool = ssd.PagePool();
	rocksdb::ocssd::oc_buffer buff(pool);

	buff.TEST_Large2(); 

	return 0;
}


int page_test()
{
	rocksdb::ocssd::oc_ssd ssd;
	rocksdb::ocssd::oc_page_pool *pool;
	rocksdb::ocssd::oc_page *page;
	if (ssd.ok()) {
		printf("construct ok.\n");
	}else{
		printf("ocssd construct failed: %s.\n", ssd.s.ToString().c_str()); 
	}
	pool = ssd.PagePool();
	if (!pool->page_alloc(&page).ok()) {
		printf("page alloc failed.\n");
		return -1;
	}
	page->TEST_Basic();
	return 0;
}

int page_pool_test()
{
	rocksdb::ocssd::oc_ssd ssd;
	rocksdb::ocssd::oc_page_pool *pool;
	if (ssd.ok()) {
		printf("construct ok.\n");
	}else{
		printf("ocssd construct failed: %s.\n", ssd.s.ToString().c_str()); 
	}
	pool = ssd.PagePool();
	pool->TEST_Basic();

	return 0;
}

int main()
{
	oc_buffer_testx();
	return 0;
}
