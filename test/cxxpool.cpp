#include <libunittest/all.hpp>
#include "../src/cxxpool.h"


COLLECTION(test_cxxpool) {

TEST(test_default_construction) {
  const cxxpool::thread_pool pool;
  ASSERT_GREATER(pool.n_threads(), 0);
}

TEST(test_construct_with_thread_number) {
  const int threads = 4;
  const cxxpool::thread_pool pool{threads};
  ASSERT_EQUAL(threads, pool.n_threads());
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
  auto future2 = pool.push([](double value) { return value; }, 2.);
  ASSERT_EQUAL(1, future1.get());
  ASSERT_EQUAL(2., future2.get());
}

TEST(test_add_various_tasks_with_priorities) {
  cxxpool::thread_pool pool{3};
  auto future1 = pool.push([]{ return 1; });
  auto future2 = pool.push(1, [](double value) { return value; }, 2.);
  auto future3 = pool.push(2, [](double a, int b) { return a * b; }, 3, 2.);
  auto future4 = pool.push(1, []{ return true; });
  ASSERT_EQUAL(1, future1.get());
  ASSERT_EQUAL(2., future2.get());
  ASSERT_EQUAL(6., future3.get());
  ASSERT_EQUAL(true, future4.get());
}

TEST(test_add_task_with_exception) {
  cxxpool::thread_pool pool;
  auto future1 = pool.push([]() -> int { throw std::bad_alloc{}; return 1; });
  auto future2 = pool.push([](double value) { return value; }, 2.);
  ASSERT_THROW(std::bad_alloc, [&future1] { future1.get(); });
  ASSERT_EQUAL(2., future2.get());
}

}
