#include <libunittest/all.hpp>
#include "../src/cxxpool.h"


COLLECTION(test_cxxpool) {


TEST(test_thread_pool_noarg_construction) {
  try {
    const cxxpool::thread_pool pool;
    ASSERT_GREATER(pool.n_threads(), 0);
  } catch (const cxxpool::thread_pool_error&) {
    // may get here on, e.g., VMs
  }
}

TEST(test_thread_pool_construct_with_thread_number) {
  const int threads = 4;
  const cxxpool::thread_pool pool{threads};
  ASSERT_EQUAL(threads, pool.n_threads());
}

TEST(test_thread_pool_add_simple_task_void) {
  cxxpool::thread_pool pool{2};
  int a = 1;
  auto future = pool.push([&a]{ a = 2; });
  future.get();
  ASSERT_EQUAL(2, a);
}

TEST(test_thread_pool_add_two_tasks) {
  cxxpool::thread_pool pool{4};
  auto future1 = pool.push([]{ return 1; });
  auto future2 = pool.push([](double value) { return value; }, 2.);
  ASSERT_EQUAL(1, future1.get());
  ASSERT_EQUAL(2., future2.get());
}

TEST(test_thread_pool_add_various_tasks_with_priorities) {
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

TEST(test_thread_pool_add_task_with_exception) {
  cxxpool::thread_pool pool{4};
  auto future1 = pool.push([]() -> int { throw std::bad_alloc{}; return 1; });
  auto future2 = pool.push([](double value) { return value; }, 2.);
  ASSERT_THROW(std::bad_alloc, [&future1] { future1.get(); });
  ASSERT_EQUAL(2., future2.get());
}

TEST(test_infinite_counter_increment_operator) {
  cxxpool::detail::infinite_counter c1;
  auto c2 = ++c1;
  ASSERT_FALSE(c1 > c2);
  ASSERT_FALSE(c2 > c1);
}

TEST(test_infinite_counter_no_increment) {
  cxxpool::detail::infinite_counter c1;
  cxxpool::detail::infinite_counter c2;
  ASSERT_FALSE(c1 > c2);
  ASSERT_FALSE(c2 > c1);
}

TEST(test_infinite_counter_one_increments) {
  cxxpool::detail::infinite_counter c1;
  cxxpool::detail::infinite_counter c2;
  ++c1;
  ASSERT_TRUE(c1 > c2);
  ASSERT_FALSE(c2 > c1);
}

TEST(test_infinite_counter_both_increment) {
  cxxpool::detail::infinite_counter c1;
  cxxpool::detail::infinite_counter c2;
  ++c1;
  ++c2;
  ASSERT_FALSE(c1 > c2);
  ASSERT_FALSE(c2 > c1);
  ++c1;
  ASSERT_TRUE(c1 > c2);
  ASSERT_FALSE(c2 > c1);
}

TEST(test_priority_task_noarg_construction) {
  cxxpool::detail::priority_task t1;
  cxxpool::detail::priority_task t2;
  ASSERT_FALSE(t1.callback());
  ASSERT_FALSE(t1 < t2);
  ASSERT_FALSE(t2 < t1);
}

template<typename T, typename... U>
size_t get_address(std::function<T(U...)> f) {
  typedef T(fn_type)(U...);
  fn_type** fn_pointer = f.template target<fn_type*>();
  return reinterpret_cast<size_t>(*fn_pointer);
}

void some_function() {}

void some_other_function() {}

TEST(test_priority_task_with_different_priorities) {
  cxxpool::detail::infinite_counter c;
  cxxpool::detail::priority_task t1{some_function, 3, c};
  ++c;
  cxxpool::detail::priority_task t2{some_function, 2, c};
  ASSERT_EQUAL(get_address(t1.callback()), get_address(t2.callback()));
  ASSERT_TRUE(t2 < t1);
  ASSERT_FALSE(t1 < t2);
}

TEST(test_priority_task_with_same_priorities) {
  cxxpool::detail::infinite_counter c;
  cxxpool::detail::priority_task t1{some_function, 2, c};
  ++c;
  cxxpool::detail::priority_task t2{some_other_function, 2, c};
  ASSERT_NOT_EQUAL(get_address(t1.callback()), get_address(t2.callback()));
  ASSERT_TRUE(t2 < t1);
  ASSERT_FALSE(t1 < t2);
}

TEST(test_priority_task_with_same_priorities_and_same_order) {
  cxxpool::detail::infinite_counter c;
  cxxpool::detail::priority_task t1{some_function, 2, c};
  cxxpool::detail::priority_task t2{some_function, 2, c};
  ASSERT_FALSE(t2 < t1);
  ASSERT_FALSE(t1 < t2);
}


}
