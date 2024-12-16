#include <tm.hpp>
#include "data-structures.hpp"
#include "helpers.hpp"

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
