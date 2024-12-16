#pragma once
// External headers
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <list>
#include <atomic>

// Internal headers
#include <tm.hpp>
#include "macros.hpp"

using namespace std;

using word = uintptr_t;

using version = uint64_t;

constexpr size_t SPIN_COUNT_MAX = 1000;
constexpr size_t NUM_LOCKS = 10000;

struct VersionedWriteLock {
    atomic<word> version_and_lock;
    VersionedWriteLock();
    bool lock();
    void unlock();
    word getVersion();
    bool isLocked();
};

struct MemorySegment {
    /**
     * @attention may need to update to store ptr to actual location
     */
    MemorySegment* next;
    MemorySegment* prev;
    MemorySegment(): next{nullptr}, prev{nullptr} {};
};

struct MemoryRegion {
    MemorySegment* seg_list;
    size_t size;
    size_t align;
    VersionedWriteLock* locks;
    void* start;
    MemoryRegion(size_t size, size_t align);
};

// struct ReadOperation {
//     // All of the locations we need to 
//     word* target;
//     word val;
//     ReadOperation(word* target_, word val_);
// };

struct WriteOperation {
    // All of the locations we need to 
    word* source;
    word val;
    WriteOperation(word* source_, word val_);
    // WriteOperation& operator=(WriteOperation&& other);
};

struct Transaction {
    version rv;
    unordered_set<word*> read_set;
    unordered_map<word*, WriteOperation> write_set;
    MemoryRegion* region;
    bool is_ro;
    Transaction(version gvc, MemoryRegion* region_, bool is_ro_);
};



