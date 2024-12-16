#include "data-structures.hpp"


Transaction::Transaction(version gvc, MemoryRegion* region_, bool is_ro_): rv{gvc}, region{region_}, is_ro{is_ro_} {}

MemoryRegion::MemoryRegion(size_t size_, size_t align_): seg_list{nullptr}, size{size_}, align{align_}, start{nullptr} {}

// ReadOperation::ReadOperation(word* target_, word val_): target{target_}, val{val_} {}

WriteOperation::WriteOperation(word* source_, word val_): source{source_}, val{val_} {}

VersionedWriteLock::VersionedWriteLock(): version_and_lock{0} {};

bool VersionedWriteLock::lock() {
    word expected, desired;
    size_t spin_count = 0;
    while (true) {  
        if (++spin_count > SPIN_COUNT_MAX) {
            return false;
        }
        // We hope the lock bit is not set
        expected = version_and_lock.load(std::memory_order_relaxed) & ~1;
        // We want the same timestamp but with the lock bit set
        desired = expected | 1;  

        if (version_and_lock.compare_exchange_weak(expected, desired, std::memory_order_acquire, std::memory_order_relaxed)) {
            // Successfully took the lock
            return true;
        }
        // If we failed then we need to wait for the lock bit to be cleared.
        while (version_and_lock.load(std::memory_order_relaxed) & 1) {
            // Spin : )
        }
    }
}


void VersionedWriteLock::unlock() {
    word current = version_and_lock.load(std::memory_order_relaxed);
    word new_value = (current & ~1u) + 2;  // Increment the version and clear the lock bit
    version_and_lock.store(new_value, std::memory_order_release);
}

word VersionedWriteLock::getVersion() {
    return version_and_lock.load(std::memory_order_relaxed) >> 1;
}

bool VersionedWriteLock::isLocked() {
    return version_and_lock.load(std::memory_order_relaxed) & 1;
}

void VersionedWriteLock::setVersion(version v) {
    word current = version_and_lock.load(std::memory_order_relaxed);
    word new_val = v << 1 | 1; // Set the version while keeping the lock locked
    version_and_lock.store(version_and_lock, std::memory_order_relaxed);
}
