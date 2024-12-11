#include "data-structures.hpp"


Transaction::Transaction(version gvc, MemoryRegion* region_, bool is_ro_): rv{gvc}, region{region_}, is_ro{is_ro_} {}

MemoryRegion::MemoryRegion(size_t size_, size_t align_): seg_list{nullptr}, size{size_}, align{align_}, start{nullptr} {}

ReadOperation::ReadOperation(word* target_, word val_, bool is_valid_): target{target_}, val{val_}, is_valid{is_valid_} {}

WriteOperation::WriteOperation(word* source_, word val_): source{source_}, val{val_} {}
WriteOperation& WriteOperation::operator=(WriteOperation&& other) {
    source = other.source;
    val = other.val;
    return *this;
}
