#include <libunittest/all.hpp>
#include "../src/cxxpool.h"


COLLECTION(test_cxxpool) {

TEST(test_default_construction) {
  const cxxpool::thread_pool<void> pool;
  ASSERT_GREATER(pool.max_n_threads(), 0);
}

TEST(test_construct_with_thread_number) {
  const int threads = 4;
  const cxxpool::thread_pool<void> pool{threads};
  ASSERT_EQUAL(threads, pool.max_n_threads());
}

TEST(test_add_simple_task) {
  cxxpool::thread_pool<void> pool;
  int a = 1;
  std::future<void> future = pool.push([&a]{ a = 2; });
  future.get();
  ASSERT_EQUAL(2, a);
}


}
