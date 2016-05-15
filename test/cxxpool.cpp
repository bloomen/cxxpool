#include <libunittest/all.hpp>
#include "../src/cxxpool.h"


COLLECTION(test_cxxpool) {

TEST(test_default_construction) {
  const cxxpool::thread_pool pool;
  ASSERT_GREATER(pool.max_n_threads(), 0);
}

TEST(test_construct_with_thread_number) {
  const int threads = 4;
  const cxxpool::thread_pool pool{threads};
  ASSERT_EQUAL(threads, pool.max_n_threads());
}

TEST(test_add_simple_task_void) {
  cxxpool::thread_pool pool;
  int a = 1;
  auto future = pool.push([&a]{ a = 2; });
  future.get();
  ASSERT_EQUAL(2, a);
}

TEST(test_add_two_tasks) {
  cxxpool::thread_pool pool;
  auto future1 = pool.push([]{ return 1; });
  auto future2 = pool.push([]{ return 2.; });
  ASSERT_EQUAL(1, future1.get());
  ASSERT_EQUAL(2., future2.get());
}

}
