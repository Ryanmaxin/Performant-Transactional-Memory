#pragma once
#include <tm.hpp>
#include "data-structures.hpp"
#include <list>

/* Validation: make sure the address we want to read from:
1. Is not locked
2. Has version <= rv
*/
bool validateRead(shared_t shared, word* addr, version rv);

version increment(version* v);

void freeHeldLocks(list<VersionedWriteLock*>& locks_held);
