#include "../include/tm.hpp"
#include <iostream>
#include <thread>

constexpr int NUM_ELEMS = 10;
constexpr int ALIGN = 8;
constexpr int SIZE = NUM_ELEMS*ALIGN;
constexpr int NUM_THREADS = 2; // Number of threads

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

void thread_work(shared_t shared_mem, int thread_id) {
    // Define transaction and operations here
    void* source = nullptr;
    void* target = new uint64_t*[SIZE]; // Assuming each element is an int for simplicity

    tx_t txn = tm_begin(shared_mem, false); // Start a transaction
    tm_align(shared_mem); // Align transaction
    tm_end(shared_mem, txn); // End transaction

    delete[] (uint64_t*)(target); // Clean up
}

int main()
{
    
    std::thread threads[NUM_THREADS];

    shared_t shared_mem = tm_create(80000, 8); // Create shared memory

    // Launch threads
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads[i] = std::thread(thread_work, shared_mem, i);
    }

    // Join threads
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads[i].join();
    }

    tm_destroy(shared_mem); // Destroy shared memory
    return 0;
}
