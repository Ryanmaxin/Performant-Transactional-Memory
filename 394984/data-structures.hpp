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
    void setVersion(version v);
    bool isLocked();
};

struct MemoryRegion {
    unordered_set<void*> master_seg_list;
    mutex list_lock;
    size_t size;
    size_t align;
    VersionedWriteLock* locks;
    void* start;
    MemoryRegion(size_t size, size_t align);
    ~MemoryRegion();
};

struct Operation {
    void* val;
    void* addr;
    Operation(char* data, size_t word_size, void* addr_);
    ~Operation();
    // WriteOperation& operator=(WriteOperation&& other);
};

struct Transaction {
    version rv;
    unordered_map<char*, unique_ptr<Operation>> read_set;
    unordered_map<char*, unique_ptr<Operation>> write_set;
    unordered_set<void*> seg_list;
    unordered_set<void*> local_only_seg_list;
    unordered_set<void*> free_seg_list;
    bool is_ro;
    Transaction(version gvc, unordered_set<void*>& seg_list_, bool is_ro_);
};



