#include <tm.hpp>
#include "data-structures.hpp"
#include "helpers.hpp"

bool validateRead(shared_t shared, word* addr, version rv) {
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);

    // Get the lock which protects the address we want to read from.
    VersionedWriteLock& lock = region->locks[(word)addr % NUM_LOCKS];

    if (lock.isLocked() || lock.getVersion() > rv + 1) {
        return false;
    }
    return true;
}
