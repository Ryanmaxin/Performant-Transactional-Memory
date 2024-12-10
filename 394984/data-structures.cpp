#include "data-structures.hpp"


Transaction::Transaction(version gvc) {
    rv = gvc;
}

MemoryRegion::MemoryRegion(size_t size_, size_t align_): seg_list{nullptr}, size{size_}, align{align_}, start{nullptr} {}
