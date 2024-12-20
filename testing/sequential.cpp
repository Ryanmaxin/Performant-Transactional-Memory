#include "../include/tm.hpp"
#include <iostream>
#include <thread>

constexpr int NUM_ELEMS = 10;
constexpr int ALIGN = 8;
constexpr int SIZE = NUM_ELEMS*ALIGN;

void printSegment(void* segment, size_t size, bool fillWithZeros = false, bool fillWithLinear = false) {
    for (size_t i = 0; i < size; i++) {
        if (fillWithZeros) {
            ((int*)segment)[i] = 0;
        } else if (fillWithLinear) {
            ((int*)segment)[i] = i;
        }
        std::cout << ((int*)segment)[i] << " ";
    }
    std::cout << std::endl;
}

int main()
{
    std::cout << "SIZE: " << SIZE << std::endl;
    void* target = new uint64_t[NUM_ELEMS];

    printSegment(target,NUM_ELEMS,false,true);
   
    shared_t shared = tm_create(80000,ALIGN);
    // void *source = nullptr;
    tx_t txn = tm_begin(shared,false);

    void* source = tm_start(shared);
    tm_alloc(shared,txn,SIZE,&source);
    printSegment(source,NUM_ELEMS);
    tm_read(shared,txn,source,NUM_ELEMS*sizeof(int),target);
    tm_write(shared,txn,target,NUM_ELEMS*sizeof(int),source);

    // tm_write(shared,txn,target,SIZE,source);  
    // tm_free(shared,txn,source);
    tm_end(shared,txn);

    printSegment(target,NUM_ELEMS);
    printSegment(source,NUM_ELEMS);

    void* target2 = new uint64_t[NUM_ELEMS];
    // tx_t txn1 = tm_begin(shared,false);
    // printSegment(target2,NUM_ELEMS);
    // tm_read(shared,txn1,source,NUM_ELEMS*sizeof(int),target2);
    // printSegment(target2,NUM_ELEMS);
    // if (!tm_free(shared,txn1,source)) {
    //     std::cout << "Failed to free" << std::endl;
    // }

    // tm_alloc(shared,txn1,SIZE,&source);
    // tm_alloc(shared,txn1,SIZE,&source);
    
    // tm_end(shared,txn1);

    tm_destroy(shared);
    delete[] (uint64_t*)target2;
    delete[] (uint64_t*)target;
    return 0;
}
