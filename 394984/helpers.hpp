#pragma once

#include <tm.hpp>
#include <list>
#include <sstream>
#include <iostream>
#include <thread>
#include "data-structures.hpp"

/* Validation: make sure the address we want to read from:
1. Is not locked
2. Has version <= rv
*/
bool validateRead(shared_t shared, char* addr, version rv, unordered_set<VersionedWriteLock*>* locks_held = nullptr) {
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);

    // Get the lock which protects the address we want to read from.
    VersionedWriteLock* lock = &region->locks[(word)addr % NUM_LOCKS];
    bool lock_owned = false;
    if (locks_held && locks_held->find(lock) != locks_held->end()) lock_owned = true;

    if ((lock_owned || !lock->isLocked()) && lock->getVersion() <= rv) return true;

    return false;
}

void cleanup(Transaction* txn) {
    for (auto seg : txn->local_only_seg_list) {
        free(seg);
    }
    delete txn;
}

void printUnorderedSet(const std::unordered_set<void*>& pointers) {
    std::cout << "Unordered Set contains:" << std::endl;
    for (void* ptr : pointers) {
        std::cout << ptr << std::endl;
    }
}


void freeHeldLocks(unordered_set<VersionedWriteLock*>& locks_held) {
    for (auto lock : locks_held) {
        lock->unlock();
    }
}

std::ostream& operator<<(std::ostream& os, Alloc alloc) {
    switch (alloc) {
        case Alloc::success:
            return os << "success";
        case Alloc::nomem:
            return os << "nomem";
        case Alloc::abort:
            return os << "abort";
        default:
            return os << "unknown";
    }
}
