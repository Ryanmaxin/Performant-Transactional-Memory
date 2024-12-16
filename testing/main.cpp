#include "../include/tm.hpp"
#include <iostream>

constexpr int NUM_ELEMS = 50;

int main()
{
    void* target = new int[NUM_ELEMS];

    for (int i = 0; i < NUM_ELEMS; i++) {
        ((int*)target)[i] = i;
        std::cout << ((int*)target)[i] << std::endl;
    }   
   
    shared_t shared = tm_create(80000,8);
    tx_t txn = tm_begin(shared,false);
    // tm_alloc(shared,txn,100*sizeof(int),&source);
    void* source = tm_start(shared);
    tm_read(shared,txn,source,NUM_ELEMS*sizeof(int),target);

    for (int i = 0; i < NUM_ELEMS; i++) {
        ((int*)target)[i] = i;
    }   

    tm_write(shared,txn,target,NUM_ELEMS*sizeof(int),source);  
    tm_end(shared,txn);
    for (int i = 0; i < NUM_ELEMS; i++) {
        std::cout << ((int*)source)[i] << std::endl;
    } 


    // txn = tm_begin(shared,true);
    // tm_end(shared,txn);
    tm_destroy(shared);
    return 0;
}
