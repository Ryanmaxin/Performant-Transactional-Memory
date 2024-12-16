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

// Internal headers
#include <tm.hpp>
#include "data-structures.hpp"
#include "macros.hpp"

// Global variables
version gvc = 0;
const size_t NUM_LOCKS = 10000;

using namespace std;
/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t size, size_t align) noexcept {
    MemoryRegion* region = new(std::nothrow) MemoryRegion(size,align);
    if (unlikely(!region)) return invalid_shared;

    // We will just allocate a large fixed number of locks, rather then locking each individual word
    region->locks = new(std::nothrow) VersionedWriteLock[NUM_LOCKS];
    if (unlikely(!region->locks)) {
        delete region;
        return invalid_shared;
    }

    // We allocate the shared memory buffer such that its words are correctly
    // aligned.
    region->start = aligned_alloc(align, size);
    if (unlikely(!region->start)) {
        delete region;
        delete[] region->locks;
        return invalid_shared;
    }
    memset(region->start, 0, size);
    return region;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) noexcept {
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);

    while (region->seg_list) {
        MemorySegment* next = region->seg_list->next;
        delete region->seg_list;
        region->seg_list = next;
    }

    free(region->start);
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
tx_t tm_begin(shared_t shared, bool is_ro) noexcept {
    // Write Transaction (1) 
    MemoryRegion* region = reinterpret_cast<MemoryRegion*>(shared);
    Transaction* txn = new(nothrow) Transaction(gvc,region,is_ro);
    if (!txn) return invalid_tx;

    /**
     * @attention may need to do more here. But leave it for now.
     */
    
    return reinterpret_cast<tx_t>(txn);
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t unused(shared), tx_t unused(tx)) noexcept {
    /**
     * @attention Need to delete the transaction here.
     */
    
    return false;
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
    if (txn->is_ro) {
        
    } else {
        // Write Transaction (2)
        // Convert void* to word* for easier manipulation
        word* target_start = (word*)(target);
        word* source_start = (word*)(source);

        // Invariant: size is a multiple of the alignment
        size_t num_words = size / tm_align(shared);

        // Go through every word we want to read from and add it to the read set
        for (size_t i = 0; i < num_words; i += 1) {
            word* source_addr = source_start + i;
            word* target_addr = target_start + i;

            word val = 0;
            bool is_valid = false;

            // Check if the address was written to previously
            auto it = txn->write_set.find(source_addr);
            if (it != txn->write_set.end()) {
                // Address was previously written to
                val = it->second.val;
                is_valid = true;
            }

            // Keep track of all of the places we will need to read to
            txn->read_set[source_addr].emplace_back(target_addr,val,is_valid);
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

    word* target_start = (word*)(target);
    word* source_start = (word*)(source);

    // Invariant: size is a multiple of the alignment
    size_t num_words = size / tm_align(shared);

    // Go through every word we want to read from and add it to the read set
    for (size_t i = 0; i < num_words; i += 1) {
        word* source_addr = source_start + i;
        word* target_addr = target_start + i;

        word val = *source_addr;
        // Keep track of all of the places we will need to read to
        txn->write_set.insert_or_assign(target_addr,WriteOperation(source_addr,val));
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
Alloc tm_alloc(shared_t unused(shared), tx_t unused(tx), size_t unused(size), void** unused(target)) noexcept {
    // TODO: tm_alloc(shared_t, tx_t, size_t, void**)
    return Alloc::abort;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t unused(shared), tx_t unused(tx), void* unused(target)) noexcept {
    // TODO: tm_free(shared_t, tx_t, void*)
    //CANT FREE INITIAL SEGMENT
    return false;
}
