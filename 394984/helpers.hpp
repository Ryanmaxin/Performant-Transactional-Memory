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
bool validateRead(shared_t shared, word* addr, version rv) {
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);

    // Get the lock which protects the address we want to read from.
    VersionedWriteLock& lock = region->locks[(word)addr % NUM_LOCKS];

    if (lock.isLocked() || lock.getVersion() > rv) {
        return false;
    }
    return true;
}


void freeHeldLocks(list<VersionedWriteLock*>& locks_held) {
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
