#pragma once

#include <tm.hpp>
#include <list>
#include <sstream>
#include <iostream>
#include <thread>
#include "data-structures.hpp"

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
