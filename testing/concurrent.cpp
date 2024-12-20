#include "../include/tm.hpp"
#include <iostream>
#include <thread>

constexpr int NUM_ELEMS = 10;
constexpr int ALIGN = 8;
constexpr int SIZE = NUM_ELEMS*ALIGN;
constexpr int NUM_THREADS = 8; // Number of threads

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
    void* data = new uint64_t*[NUM_ELEMS];
    printSegment(data, NUM_ELEMS, false,true);

    void* segment = tm_start(shared_mem); // Start transaction
    tx_t txn = tm_begin(shared_mem, false); // Start a transaction
    std::this_thread::yield();
    tm_write(shared_mem, txn, data, SIZE, segment); // Write to shared memory
    std::this_thread::yield();
    printSegment(segment, NUM_ELEMS, false, false);
    tm_end(shared_mem, txn); // End transaction

    delete[] (uint64_t*)(data); // Clean up
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
