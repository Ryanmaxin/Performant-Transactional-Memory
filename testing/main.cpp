#include <tm.hpp>
int main(int argc, char const *argv[])
{
    shared_t shared = tm_create(128,4);
    tm_destroy(shared);
    return 0;
}
