#include "../include/tm.hpp"
int main()
{
    shared_t shared = tm_create(128,8);
    tx_t txn = tm_begin(shared,false);
    tm_end(shared,txn);
    txn = tm_begin(shared,true);
    tm_end(shared,txn);
    tm_destroy(shared);
    return 0;
}
