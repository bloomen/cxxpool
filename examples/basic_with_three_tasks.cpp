#include "basic_with_three_tasks.h"
#include "../src/cxxpool.h"
#include <iostream>

namespace examples {

int sum(int x, int y) {
    return x + y;
}

// This examples creates a thread pool with 4 threads and pushes
// three simple tasks into the pool. The push() function returns
// a future associated to the underlying execution.
void basic_with_three_tasks(std::ostream& os) {
    cxxpool::thread_pool pool{4};

    // pushing tasks and retrieving futures
    auto future1 = pool.push([]{ return 42; });
    auto future2 = pool.push([](double x){ return x; }, 13.);
    auto future3 = pool.push(sum, 6, 7);

    // output: results = 42, 13, 13
    os << "results = " << future1.get() << ", ";
    os << future2.get() << ", " << future3.get() << std::endl;
}

}

#ifndef USE_LIBUNITTEST
int main() {
    std::cout << "Running example: basic_with_three_tasks ..." << std::endl;
    examples::basic_with_three_tasks(std::cout);
}
#endif
