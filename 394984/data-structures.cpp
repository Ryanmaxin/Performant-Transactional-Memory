#include "data-structures.hpp"
#include <sstream>
#include <iostream>

Transaction::Transaction(version gvc, bool is_ro_): rv{gvc}, is_ro{is_ro_} {}

Transaction::~Transaction() {
    // Free all of the segments and clear the list
    for (auto& seg : seg_list) {
        free(seg);
    }
    seg_list.clear();
}

MemoryRegion::MemoryRegion(size_t size_, size_t align_): size{size_}, align{align_}, locks{nullptr}, start{nullptr} {}

MemoryRegion::~MemoryRegion() {
    // Free all of the segments and clear the list
    for (auto& seg : seg_list) {
        free(seg);
    }
    seg_list.clear();
    delete[] locks;
    free(start);
}

WriteOperation::WriteOperation(char* data, size_t word_size): val{aligned_alloc(word_size, word_size)} {
    memcpy(val, data, word_size);
}

WriteOperation::~WriteOperation() {
    free(val);
}

VersionedWriteLock::VersionedWriteLock(): version_and_lock{0} {};

bool VersionedWriteLock::lock() {
    word expected, desired;
    // We hope the lock bit is not set
    expected = version_and_lock.load() & ~1;
    // We want the same timestamp but with the lock bit set
    desired = expected | 1;  

    if (version_and_lock.compare_exchange_weak(expected, desired)) {
        // Successfully took the lock
        return true;
    }
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
