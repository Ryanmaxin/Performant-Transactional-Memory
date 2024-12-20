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

// Internal headers
#include <tm.hpp>
#include "helpers.hpp"
#include "data-structures.hpp"
#include "macros.hpp"

// Custom print function
template <typename... Args>
void dprint(Args&&... args) {
    // std::ostringstream oss;
    // oss << "[Thread " << std::this_thread::get_id() << "] ";
    // (oss << ... << args); // Fold expression to handle multiple arguments
    // std::cout << oss.str() << std::endl;
}
// Custom print function
template <typename... Args>
void dprint2(Args&&... args) {
    // std::ostringstream oss;
    // oss << "[Thread " << std::this_thread::get_id() << "] ";
    // (oss << ... << args); // Fold expression to handle multiple arguments
    // std::cout << oss.str() << std::endl;
}

// Global variables
atomic<version> gvc{0};

using namespace std;
/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t size, size_t align) noexcept {
    dprint("[CALL] tm_create(",size,",",align,")");
    MemoryRegion* region = new(std::nothrow) MemoryRegion(size,align);
    if (unlikely(!region)) return invalid_shared;

    // We will just allocate a large fixed number of locks, rather then locking each individual word
    /**
     * @attention Check if this has worse performance
     */
    region->locks = new(std::nothrow) VersionedWriteLock[NUM_LOCKS];
    if (unlikely(!region->locks)) {
        delete region;
        dprint("[FAIL1] tm_create(",size,",",align,") -> ",invalid_shared);
        return invalid_shared;
    }

    // We allocate the shared memory buffer such that its words are correctly
    // aligned.
    region->start = aligned_alloc(align, size);
    if (unlikely(!region->start)) {
        dprint("[FAIL2] tm_create(",size,",",align,") -> ",invalid_shared);
        delete[] region->locks;
        delete region;
        return invalid_shared;
    }
    memset(region->start, 0, size);
    dprint("[RETURN] tm_create(",size,",",align,") -> ",region);
    return region;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) noexcept {
    dprint("[CALL] tm_destroy(",shared,")");
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);

    dprint("[RETURN] tm_destroy(",shared,")");
    delete region; 
}

/** [thread-safe] Return the start address of the first allocated segment in the shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
**/
void* tm_start(shared_t shared) noexcept {
    /**
    * @warning Maybe I need to align the memory segment somehow?
    **/
    dprint("[CALL] tm_start(",shared,")");
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);
    dprint("[RETURN] tm_start(",shared,") -> ",region->start);
    return region->start;
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
**/
size_t tm_size(shared_t shared) noexcept {
    dprint("[CALL] tm_size(",shared,")");
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);
    dprint("[RETURN] tm_size(",shared,") -> ",region->size);
    return region->size;
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
**/
size_t tm_align(shared_t shared) noexcept {
    // dprint("[CALL] tm_align(",shared,")");
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);
    // dprint("[RETURN] tm_align(",shared,") -> ",region->align);
    return region->align;
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
**/
tx_t tm_begin(shared_t shared, bool is_ro) noexcept {
    dprint("[CALL] tm_begin(",shared,",",is_ro,")");
    // Write Transaction (1) 
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);
    // We need to lock the master_seg_list, as we are copying from it.
    region->list_lock.lock();
    Transaction* txn = new(nothrow) Transaction(gvc.load(),region->master_seg_list,is_ro);
    region->list_lock.unlock();
    if (!txn) return invalid_tx;

    /**
     * @attention may need to do more here. But leave it for now.
     */
    dprint("[RETURN] tm_begin(",shared,",",is_ro,") -> ",txn);
    return reinterpret_cast<tx_t>(txn);
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t unused(shared), tx_t tx) noexcept {
    dprint("[CALL] tm_end(",shared,",",tx,")");
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);
    Transaction *txn = reinterpret_cast<Transaction*>(tx);
    
    unordered_set<VersionedWriteLock*> locks_held;

    /**
     * @attention A possible optimization is to move onto the next lock if we fail to acquire the current one
     */

    if (!txn->is_ro) {
        // (3) Lock the write-set
        for (auto& keyval : txn->write_set) {
            char* target_addr = keyval.first;
            VersionedWriteLock* lock = &region->locks[(word)target_addr % NUM_LOCKS];
            if (!lock->lock() && locks_held.find(lock) == locks_held.end()) {
                // Here we must delete all previously held locks
                freeHeldLocks(locks_held);
                cleanup(txn);
                dprint2("TM_END: failed to acquire locks");
                return false;
            }
            locks_held.insert(lock);
        }
        // Now we have every lock we need
        // (4) Increment the global version-clock
        version wv = gvc.fetch_add(1) + 1;

        // (5) Validate the read-set
        if (txn->rv + 1 != wv) {
            for (auto& read : txn->read_set) {
                void* source_addr = read.first;

                // Get the lock which protects the address we want to read from.
                VersionedWriteLock* lock = &region->locks[(word)source_addr % NUM_LOCKS];
                bool lock_owned = (locks_held.find(lock) != locks_held.end());
                if ((!lock_owned && lock->isLocked()) || lock->getVersion() > txn->rv) {
                    // Here we must delete all previously held locks
                    freeHeldLocks(locks_held);
                    cleanup(txn);
                    dprint2("TM_END: read-set not valid");
                    return false;
                }
            }   
        }

        size_t word_size = tm_align(shared);

        // for (auto& read : txn->read_set) {
        //     void* target_addr = read.second->addr;
        //     void* val = read.second->val;
        //     memcpy(target_addr,val,word_size);
        // }
        
        // (6) Commit and release the locks
        // Commit all of our writes to the shared memory

        dprint2("COMMITING WRITE");

        for (auto& keyval : txn->write_set) {
            void* target_addr = keyval.first;
            void* val = keyval.second->val;

            // dprint2("Writing ",val," to ",target_addr);
            memcpy(target_addr,val,word_size);
            VersionedWriteLock* lock = &region->locks[(word)target_addr % NUM_LOCKS];
            lock->setVersion(wv);
        }

        freeHeldLocks(locks_held);
    

        // We need to acquire the lock for the master_seg_list before we work on it.
        region->list_lock.lock();
        // Now we free all of the memory segments we allocated
        for (auto& seg : txn->seg_list) {
            // If segment is not in the master list, add it
            auto it = region->master_seg_list.find(seg);
            if (it == region->master_seg_list.end()) region->master_seg_list.insert(seg);

        }
        for (auto& seg : txn->free_seg_list) {
            // If segment is in the master list, remove it
            auto it = region->master_seg_list.find(seg);
            if (it != region->master_seg_list.end()) region->master_seg_list.erase(it);
        }

        for (auto& seg : txn->free_seg_list) {
            free(seg);
        }

        // Then unlock the master_seg_list
        region->list_lock.unlock();
    }

    // Transaction successful, cleanup and return
    delete txn;
    dprint("[RETURN] tm_end(",shared,",",tx,") -> true");
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
    dprint("[CALL] tm_read(",shared,",",tx,",",source,",",size,",",target,")");
    Transaction *txn = reinterpret_cast<Transaction*>(tx);
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);

    // Convert void* to word* for easier manipulation
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

            dprint2("Reading ",*(word*)source_addr," from ",(word*)source_addr);
            // Prevalidate read

            // Get the lock which protects the address we want to read from.
            VersionedWriteLock* lock = &region->locks[(word)source_addr % NUM_LOCKS];
            word version = lock->getVersion();
            if (lock->isLocked() || version > txn->rv) {
                dprint2("Failed prevalidate",version,txn->rv);
                cleanup(txn);
                return false;
            }

            memcpy(target_addr,source_addr,word_size);
            
            // Get the lock which protects the address we want to read from.
            word new_version = lock->getVersion();
            if (lock->isLocked() || new_version != version || new_version > txn->rv) {
                dprint2("Failed postvalidate",lock->isLocked(),new_version !=version,new_version > txn->rv);
                cleanup(txn);
                return false;
            }
        }
    } else {
        // Write Transaction (2)

        // Go through every word we want to read and post validate it
        for (size_t i = 0; i < size; i += word_size) {
            char* source_addr = source_start + i;
            char* target_addr = target_start + i;

            // Get the lock which protects the address we want to read from.
            VersionedWriteLock* lock = &region->locks[(word)source_addr % NUM_LOCKS];

            word version = lock->getVersion();
            if (lock->isLocked() || version > txn->rv) {
                dprint2("Failed prevalidate HERE");
                cleanup(txn);
                return false;
            }

            
            // Check if the address was written to previously.
            // This will determine if we need to read from the write set or the shared memory region
            char* val_addr = nullptr;
            auto it = txn->write_set.find(source_addr);
            if (it != txn->write_set.end()) val_addr = (char*)it->second->val;
            else val_addr = source_addr;

            memcpy(target_addr,val_addr,word_size);

            word new_version = lock->getVersion();
            if (lock->isLocked() || new_version != version) {
                dprint2("Failed postvalidate HERE");
                cleanup(txn);
                return false;
            }

            // Keep track of all of the places we read from
            txn->read_set.insert_or_assign(source_addr,make_unique<Operation>(val_addr,word_size,target_addr));
        }
    }
    dprint("[RETURN] tm_read(",shared,",",tx,",",source,",",size,",",target,") -> true");
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
    dprint("[CALL] tm_write(",shared,",",tx,",",source,",",size,",",target,")");

    // Write Transaction (2)
    Transaction *txn = reinterpret_cast<Transaction*>(tx);

    char* target_start = (char*)(target);
    char* source_start = (char*)(source);

    // Invariant: size is a multiple of the alignment
    size_t word_size = tm_align(shared);

    // Go through every word we want to write to and add it to the write set
    for (size_t i = 0; i < size; i += word_size) {
        char* source_addr = source_start + i;
        char* target_addr = target_start + i;

        dprint2("Writing ",*(word*)source_addr," to ",(word*)target_addr);
        // Check if the data we want to write was read to already (overwritten)
        // This will determine if we need to read from the read set or the shared memory region
        char* val_addr = nullptr;
        bool found = false;
        // for (auto& read : txn->read_set) {
        //     if (read.second->addr == source_addr) {
        //         val_addr = (char*)read.second->val;
        //         found = true;
        //         break;
        //     }
        // }
        if (!found) val_addr = source_addr;

        
        // Keep track of all of the places we will need to write to
        txn->write_set.insert_or_assign(target_addr,make_unique<Operation>(val_addr,word_size,source_addr));
    }
    dprint("[RETURN] tm_write(",shared,",",tx,",",source,",",size,",",target,") -> true");
    return true;
}

/** [thread-safe] Memory allocation in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param size   Allocation requested size (in bytes), must be a positive multiple of the alignment
 * @param target Pointer in private memory receiving the address of the first byte of the newly allocated, aligned segment
 * @return Whether the whole transaction can continue (success/nomem), or not (abort_alloc)
**/
Alloc tm_alloc(shared_t unused(shared), tx_t tx, size_t size, void** target) noexcept {
    dprint("[CALL] tm_alloc(",shared,",",tx,",",size,",",target,")");
    Transaction *txn = reinterpret_cast<Transaction*>(tx);


    void* new_seg = aligned_alloc(tm_align(shared), size);
    if (unlikely(!new_seg)) {
        dprint("[FAIL] tm_alloc(",shared,",",tx,",",size,",",target,") -> ",Alloc::nomem);
        return Alloc::nomem;
    }

    // Add the new segment to the local seg_list
    txn->seg_list.insert(new_seg);
    txn->local_only_seg_list.insert(new_seg);

    memset(new_seg, 0, size);

    *target = new_seg;

    dprint("[RETURN] tm_alloc(",shared,",",tx,",",size,",",target,") -> ",Alloc::success);
    return Alloc::success;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t unused(shared), tx_t tx, void* unused(target)) noexcept {
    dprint("[CALL] tm_free(",shared,",",tx,",",target,")");
    Transaction *txn = reinterpret_cast<Transaction*>(tx);
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);

    // Can't free initial segment
    if (target == region->start) return false;
    // Segment must exist
    auto it = txn->seg_list.find(target);
    if (it == txn->seg_list.end()) return false;
    // Move the segment to the free_seg_list, and erase it from the original list
    txn->free_seg_list.insert(*it);
    txn->seg_list.erase(it);

    dprint("[RETURN] tm_free(",shared,",",tx,",",target,") -> true");
    return true;
}
