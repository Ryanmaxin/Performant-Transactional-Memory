#pragma once

// External headers
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <list>
#include <atomic>
#include <mutex>

// Internal headers
#include <tm.hpp>
#include "macros.hpp"

using namespace std;

using word = uintptr_t;

using version = uint64_t;

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
    list<void*> seg_list;
    mutex list_lock;
    size_t size;
    size_t align;
    VersionedWriteLock* locks;
    void* start;
    MemoryRegion(size_t size, size_t align);
    ~MemoryRegion();
};

struct WriteOperation {
    void* val;
    WriteOperation(char* data, size_t word_size);
    ~WriteOperation();
};

struct Transaction {
    version rv;
    unordered_set<char*> read_set;
    unordered_map<char*, unique_ptr<WriteOperation>> write_set;
    list<void*> seg_list;
    bool is_ro;
    Transaction(version gvc, bool is_ro_);
    ~Transaction();
};



