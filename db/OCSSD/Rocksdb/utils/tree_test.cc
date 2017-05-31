#include "oc_tree.h"
#include <cstdio>

void test1()
{
	rocksdb::ocssd::leaf_node ln; 
	rocksdb::ocssd::non_leaf_node nln;
	printf("leaf_node:%zu\nnon_leaf_node:%zu\n", sizeof(rocksdb::ocssd::leaf_node), sizeof(rocksdb::ocssd::non_leaf_node));
}


int main()
{
	test1();
	return 0;
}
