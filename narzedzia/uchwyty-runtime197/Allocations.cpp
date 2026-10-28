// Test-only allocation counter. Separate TU avoids GCC's false positive
// mismatched-new-delete diagnostic after inlining replacement delete/free.
#include <atomic>
#include <cstdlib>
#include <new>
std::atomic<unsigned long> allocations{0};
void*operator new(std::size_t n){++allocations;void*p=std::malloc(n?n:1);if(!p)std::abort();return p;}
void operator delete(void*p)noexcept{std::free(p);}
void*operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void*p)noexcept{::operator delete(p);}
