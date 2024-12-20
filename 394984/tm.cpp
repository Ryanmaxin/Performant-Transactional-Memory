/**
 * @file   tm.cpp
 * @author Ryan Maxin
 *
 * @section LICENSE
 *
 * [...]
 *
 * @section DESCRIPTION
 *
 * Implementation of your own transaction manager.
 * You can completely rewrite this file (and create more files) as you wish.
 * Only the interface (i.e. exported symbols and semantic) must be preserved.
**/

// Requested features
#define _GNU_SOURCE
#define _POSIX_C_SOURCE   200809L
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif

// External headers
#include <unordered_set>
#include <unordered_map>
#include <cstdlib>
#include <list>
#include <atomic>
#include <thread>
#include <mutex>
#include <cstring>

// Internal headers
#include <tm.hpp>
#include "data-structures.hpp"
#include "macros.hpp"

// Global variables
// This is our global version clock.
atomic<version> gvc{0};

using namespace std;
/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t size, size_t align) noexcept {
    MemoryRegion* region = new(std::nothrow) MemoryRegion(size,align);
    if (unlikely(!region)) return invalid_shared;

    // We will just allocate a large fixed number of locks, rather then locking each individual word to reduce the amount of time it takes to initialize the library.
    region->locks = new(std::nothrow) VersionedWriteLock[NUM_LOCKS];
    if (unlikely(!region->locks)) {
        delete region;
        return invalid_shared;
    }

    // We allocate the shared memory buffer such that its words are correctly
    // aligned.
    region->start = aligned_alloc(align, size);
    if (unlikely(!region->start)) {
        delete[] region->locks;
        delete region;
        return invalid_shared;
    }
    // As required, we zero out the memory
    memset(region->start, 0, size);
    return region;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) noexcept {
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);

    //destructor will do most of the work here
    delete region; 
}

/** [thread-safe] Return the start address of the first allocated segment in the shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
**/
void* tm_start(shared_t shared) noexcept {
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);
    return region->start;
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
**/
size_t tm_size(shared_t shared) noexcept {
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);
    return region->size;
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
**/
size_t tm_align(shared_t shared) noexcept {
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);
    return region->align;
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
**/
tx_t tm_begin(shared_t unused(shared), bool is_ro) noexcept {
    // Write Transaction (1) 
    Transaction* txn = new(nothrow) Transaction(gvc.load(),is_ro);
    if (!txn) return invalid_tx;

    return reinterpret_cast<tx_t>(txn);
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t unused(shared), tx_t tx) noexcept {
    // dprint("[CALL] tm_end(",shared,",",tx,")");
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);
    Transaction *txn = reinterpret_cast<Transaction*>(tx);

    // Keep track of all the locks we are currently holding
    unordered_set<VersionedWriteLock*> locks_held;

    // A possible optimization is to move onto the next lock if we fail to acquire the current one. But we won't do that here.

    // We can skip most of the work if it is a readonly transaction
    if (!txn->is_ro) {
        // (3) Lock the write-set
        for (auto& keyval : txn->write_set) {
            char* target_addr = keyval.first;
            VersionedWriteLock* lock = &region->locks[(word)target_addr % NUM_LOCKS];
            if (!lock->lock() && locks_held.find(lock) == locks_held.end()) {
                // Here we must delete all previously held locks and cleanup
                for (auto lock : locks_held) {
                    lock->unlock();
                }
                delete txn;
                return false;
            }
            locks_held.insert(lock);
        }
        // Now we have every lock we need

        // (4) Increment the global version-clock
        // Note: You need to add + 1 here! You would think it would return the incremented value but it doesn't. I think this one thing caused me up to 4 hours of debugging : (
        version wv = gvc.fetch_add(1) + 1;

        // (5) Validate the read-set (only if someone has touched the gvc since the transaction started)
        if (txn->rv + 1 != wv) {
            for (auto& read : txn->read_set) {
                // Get the lock which protects the address we want to read from.
                VersionedWriteLock* lock = &region->locks[(word)read % NUM_LOCKS];
                bool lock_owned = (locks_held.find(lock) != locks_held.end());

                // If the lock is owned, it must be owned by us.
                if ((!lock_owned && lock->isLocked()) || lock->getVersion() > txn->rv) {
                    // Here we must delete all previously held locks and cleanup
                    for (auto lock : locks_held) {
                        lock->unlock();
                    }
                    delete txn;
                    return false;
                }
            }   
        }

        size_t word_size = tm_align(shared);
        
        // (6) Commit and release the locks
        for (auto& keyval : txn->write_set) {
            void* target_addr = keyval.first;
            void* val = keyval.second->val;

            memcpy(target_addr,val,word_size);
            VersionedWriteLock* lock = &region->locks[(word)target_addr % NUM_LOCKS];
            // setVersion also unlocks the lock
            lock->setVersion(wv);
        }

        // Finally add the allocations from this transaction to the shared segment_list
        region->list_lock.lock();
        region->seg_list.splice(region->seg_list.end(), txn->seg_list);
        region->list_lock.unlock(); 
    }

    // Transaction successful, cleanup and return
    delete txn;
    return true;
}

/** [thread-safe] Read operation in the given transaction, source in the shared region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
bool tm_read(shared_t shared, tx_t tx, void const* source, size_t size, void* target) noexcept {
    Transaction *txn = reinterpret_cast<Transaction*>(tx);
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);

    // Convert void* to char* for bytewise manipulation
    char* target_start = (char*)(target);
    char* source_start = (char*)(source);

    // Invariant: size is a multiple of the alignment
    size_t word_size = tm_align(shared);

    if (txn->is_ro) {
        // Low-Cost Read-Only Transaction
        // (2) Run through a speculative execution
        for (size_t i = 0; i < size; i += word_size) {
            char* source_addr = source_start + i;
            char* target_addr = target_start + i;

            // Pre validate read
            VersionedWriteLock* lock = &region->locks[(word)source_addr % NUM_LOCKS];
            word version = lock->getVersion();
            if (lock->isLocked() || version > txn->rv) {
                delete txn;
                return false;
            }

            memcpy(target_addr,source_addr,word_size);

            // Post validate read
            word new_version = lock->getVersion();
            if (lock->isLocked() || new_version != version || new_version > txn->rv) {
                delete txn;
                return false;
            }
        }
    } else {
        // Write Transaction (2)

        // Go through every word we want to read from and pre/post validate it
        for (size_t i = 0; i < size; i += word_size) {
            char* source_addr = source_start + i;
            char* target_addr = target_start + i;

            // Get the lock which protects the address we want to read from.
            VersionedWriteLock* lock = &region->locks[(word)source_addr % NUM_LOCKS];

            // Pre validate read
            word version = lock->getVersion();
            if (lock->isLocked() || version > txn->rv) {
                //dprint2("Failed prevalidate HERE");
                delete txn;
                return false;
            }

            
            // Check if the address was written to previously.
            // This will determine if we need to read from the write set or the shared memory region
            char* val_addr = nullptr;
            auto it = txn->write_set.find(source_addr);
            if (it != txn->write_set.end()) val_addr = (char*)it->second->val;
            else val_addr = source_addr;

            // We also copy the value directly. This technically breaks isolation, but we don't care since the value will be ignored if we later find out that the transaction must abort
            memcpy(target_addr,val_addr,word_size);

            // Post validate read
            word new_version = lock->getVersion();
            if (lock->isLocked() || new_version != version) {
                //dprint2("Failed postvalidate HERE");
                delete txn;
                return false;
            }

            // Keep track of all of the places we read from
            txn->read_set.insert(source_addr);
        }
    }
    return true;
}

/** [thread-safe] Write operation in the given transaction, source in a private region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
**/
bool tm_write(shared_t shared, tx_t tx, void const* source, size_t size, void* target) noexcept {

    // Write Transaction (2)
    Transaction *txn = reinterpret_cast<Transaction*>(tx);

    // Convert to char* for easy bytewise manipulation
    char* target_start = (char*)(target);
    char* source_start = (char*)(source);

    // Invariant: size is a multiple of the alignment
    size_t word_size = tm_align(shared);

    // Go through every word we want to write to and add it to the write set
    for (size_t i = 0; i < size; i += word_size) {
        char* source_addr = source_start + i;
        char* target_addr = target_start + i;

        // Keep track of all of the places we will need to write to
        // This WriteOperation implicitly stores the value we want to write in dynamic memory, as we don't know how big it will be until the alignment is given.
        txn->write_set.insert_or_assign(target_addr,make_unique<WriteOperation>(source_addr,word_size));
    }
    return true;
}

/** [thread-safe] Memory allocation in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param size   Allocation requested size (in bytes), must be a positive multiple of the alignment
 * @param target Pointer in private memory receiving the address of the first byte of the newly allocated, aligned segment
 * @return Whether the whole transaction can continue (success/nomem), or not (abort_alloc)
**/
Alloc tm_alloc(shared_t shared, tx_t tx, size_t size, void** target) noexcept {
    Transaction *txn = reinterpret_cast<Transaction*>(tx);


    void* new_seg = aligned_alloc(tm_align(shared), size);
    if (unlikely(!new_seg)) return Alloc::nomem;

    // Zero out the new allocation, as required
    memset(new_seg, 0, size);

    // Add the segment to the local transaction seg
    // This means if the transaction aborts we can free it without any other transactions seeing it.
    txn->seg_list.push_back(new_seg);

    *target = new_seg;

    return Alloc::success;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t unused(shared), tx_t unused(tx), void* unused(target)) noexcept {
    // We just keep tm_free as a dummy function in this implementation. The result is that successful transactions slowly build up memory until we call tm_destroy.
    // I did try other implementations where we would keep track of everything and have full open memory, but it slowed the implementation down a ton by having to lock global data structures.
    return true;
}
