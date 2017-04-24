//Dependencies headers
#include "oc_ssd.h"
#include "oc_block_manager.h"


#include "oc_file.h"

#include <malloc.h>

namespace rocksdb {

    namespace ocssd {


        oc_file::oc_file(oc_ssd *ssd, const char *fname)
                : held_by_(ssd),
                  geo_(ssd->Geo()),
                  held_by_blkmng_(ssd->Blkmng()),
                  meta_(new oc_file::metadata(fname)) {
            dummy_stphead_.next = dummy_stphead_.prev = &dummy_stphead_;
        }

        oc_file::~oc_file() {
            delete meta_;
        }

        rocksdb::Status oc_file::Append() {
        }

        rocksdb::Status oc_file::RandomReadByOffset() //Offset is byte-oriented.
        {
        }

        rocksdb::Status oc_file::Flush() {
        }

        oc_file::Node *oc_file::AllocNode(int degree) {
            oc_file::Node *ptr = (oc_file::Node *) malloc(
                    sizeof(oc_file::Node) + (degree - 1) * sizeof(oc_file::StripeDescriptor *));
            if (!ptr) {
                s = rocksdb::Status::IOError("oc_file::AllocNode", strerror(errno));
                return NULL;
            }
            ptr->degree = degree;
            ptr->size = 0;
            ptr->used = 0;
            return ptr;
        }

        void oc_file::FreeNode(oc_file::Node *n) {
            if (n) {
                free(n);
            }
        }


        ssize_t oc_file::AddStripe2Node(oc_file::Node *n, oc_file::StripeDescriptor *stripe) {
            if (NodeFull(n)) {
                return -1;
            }
            size_t blks = stripe->ed.Minus(stripe->st, geo_);
            size_t blksize = held_by_blkmng_->ChunkSize();
            n->size += blks * blksize;
            n->stp_arr[n->used] = stripe;
            n->used++;

            return blks * blksize;
        }

        oc_file::Node *oc_file::AddListTail(oc_file::Node *listhead, oc_file::Node *n) {
            n->prev = listhead->prev;
            n->next = listhead;

            listhead->prev->next = n;
            listhead->prev = n;
            return n;
        }

        void oc_file::TravelList(oc_file::Node *listhead, const char *job) {

        }


        void oc_file::TEST_NodeList() {

        }


        rocksdb::Status oc_file::New_oc_file(oc_ssd *ssd, const char *filename, oc_file **oc_file_ptr) {
            oc_file *ptr = new oc_file(ssd, filename);
            *oc_file_ptr = ptr->ok() ? ptr : NULL;
            return ptr->s;
        }
    } //namespace ocssd
} //namespcae rocksdb
