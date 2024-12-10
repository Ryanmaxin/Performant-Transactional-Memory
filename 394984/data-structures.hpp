// External headers
#include <unordered_set>
#include <unordered_map>

// Internal headers
#include <tm.hpp>
#include "macros.hpp"

using namespace std;

using tmaddr = uintptr_t;

using version = uint64_t;

class Transaction {
    version rv;
    unordered_set<tmaddr> read_set;
    unordered_map<tmaddr, int> write_set;
    public:
        Transaction(version gvc);
};
