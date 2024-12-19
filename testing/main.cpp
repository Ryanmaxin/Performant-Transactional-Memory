#include "../include/tm.hpp"
#include <iostream>

constexpr int NUM_ELEMS = 50;
constexpr int ALIGN = 8;
constexpr int SIZE = NUM_ELEMS*ALIGN;

int main()
{
    std::cout << "SIZE: " << SIZE << std::endl;
    void* target = new int[NUM_ELEMS];

    for (int i = 0; i < NUM_ELEMS; i++) {
        ((int*)target)[i] = i;
        std::cout << "TARGET: " << ((int*)target)[i] << std::endl;
    }   
   
    shared_t shared = tm_create(80000,ALIGN);
    void *source = nullptr;
    tx_t txn = tm_begin(shared,false);

    // void* source = tm_start(shared);
    tm_alloc(shared,txn,SIZE,&source);
    tm_read(shared,txn,source,NUM_ELEMS*sizeof(int),target);

    std::cout <<"source: " << source << std::endl;

    for (int i = 0; i < NUM_ELEMS; i++) {
        ((int*)target)[i] = i;
        std::cout << "TARGET: " << ((int*)target)[i] << std::endl;
    }   
    tm_write(shared,txn,target,SIZE,source);  
    // tm_free(shared,txn,source);
    tm_end(shared,txn);

    for (int i = 0; i < NUM_ELEMS; i++) {
        std::cout << "SOURCE: " << ((int*)source)[i] << std::endl;
    } 

    tm_begin(shared,true);
    tm_free(shared,txn,source);
    tm_end(shared,txn);

    tm_destroy(shared);
    return 0;
}
