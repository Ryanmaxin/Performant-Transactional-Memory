#include <tm.hpp>
#include <data-structures.hpp>

/* Validation: make sure the address we want to read from:
1. Is not locked
2. Has version <= rv + 1 
*/
bool validateRead(shared_t shared, word* addr, version rv);
