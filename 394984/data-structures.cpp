#include "data-structures.hpp"
#include <sstream>
#include <iostream>
#include <cstring>

Transaction::Transaction(version gvc, bool is_ro_): rv{gvc}, is_ro{is_ro_} {}

Transaction::~Transaction() {
    // Free all of the segments so that they don't appear to the other transactions
    for (auto& seg : seg_list) {
        free(seg);
    }
    seg_list.clear();
}

MemoryRegion::MemoryRegion(size_t size_, size_t align_): size{size_}, align{align_}, locks{nullptr}, start{nullptr} {}

MemoryRegion::~MemoryRegion() {
    // Free all of the segments so when we destroy the TM object
    for (auto& seg : seg_list) {
        free(seg);
    }
    seg_list.clear();
    // Remove all of the locks
    delete[] locks;

    // Delete the initial memory segment
    free(start);
}

WriteOperation::WriteOperation(char* data, size_t word_size): val{aligned_alloc(word_size, word_size)} {
    // val holds the data we want to write
    memcpy(val, data, word_size);
}

WriteOperation::~WriteOperation() {
    free(val);
}

VersionedWriteLock::VersionedWriteLock(): version_and_lock{0} {};

bool VersionedWriteLock::lock() {
    // A possible optimization is to spin while continually checking the lock. I found that in our case it is better to just restart the transaction.
    word expected, desired;
    // We hope the lock bit is not set
    expected = version_and_lock.load() & ~1;
    // We want the same timestamp but with the lock bit set
    desired = expected | 1;  

    if (version_and_lock.compare_exchange_weak(expected, desired)) {
        // Successfully took the lock
        return true;
    }
    // We failed to take the lock, so exit
    return false;
}


void VersionedWriteLock::unlock() {
    word current = version_and_lock.load();
    word new_value = (current & ~1u);  // Clear the lock bit
    version_and_lock.store(new_value);
}

word VersionedWriteLock::getVersion() {
    return version_and_lock.load() >> 1;
}

bool VersionedWriteLock::isLocked() {
    return version_and_lock.load() & 0x1;
}

void VersionedWriteLock::setVersion(version v) {
    word new_val = v << 1 | 0; // Set the version and unlock
    version_and_lock.store(new_val);
}
