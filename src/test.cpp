#include <libunittest/all.hpp>
#include "../src/cxxpool.h"
#include "../examples/basic_with_three_tasks.h"
#include <list>


COLLECTION(test_cxxpool) {


class condvar {
 public:

  condvar()
  : flag_{false}, cond_var_{}, mutex_{}
  {}

  void notify_one() {
    make_flag_true();
    cond_var_.notify_one();
  }

  void notify_all() {
    make_flag_true();
    cond_var_.notify_all();
  }

  void wait() {
    std::unique_lock<std::mutex> lock{mutex_};
    cond_var_.wait(lock, [this]() { return flag_; });
  }

 private:

  void make_flag_true() {
    std::lock_guard<std::mutex> lock{mutex_};
    flag_ = true;
  }

  bool flag_;
  std::condition_variable cond_var_;
  std::mutex mutex_;
};


TEST(test_thread_pool_noarg_construction) {
  const cxxpool::thread_pool pool;
  ASSERT_EQUAL(0u, pool.n_threads());
}

TEST(test_thread_pool_construct_with_thread_number) {
  const std::size_t threads = 4;
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
  cxxpool::detail::infinite_counter<int> c1;
  auto c2 = ++c1;
  ASSERT_FALSE(c1 > c2);
  ASSERT_FALSE(c2 > c1);
}

TEST(test_infinite_counter_no_increment) {
  cxxpool::detail::infinite_counter<std::uint64_t> c1;
  cxxpool::detail::infinite_counter<std::uint64_t> c2;
  ASSERT_FALSE(c1 > c2);
  ASSERT_FALSE(c2 > c1);
}

TEST(test_infinite_counter_one_increments) {
  cxxpool::detail::infinite_counter<int> c1;
  cxxpool::detail::infinite_counter<int> c2;
  ++c1;
  ASSERT_TRUE(c1 > c2);
  ASSERT_FALSE(c2 > c1);
}

TEST(test_infinite_counter_both_increment) {
  cxxpool::detail::infinite_counter<int> c1;
  cxxpool::detail::infinite_counter<int> c2;
  ++c1;
  ++c2;
  ASSERT_FALSE(c1 > c2);
  ASSERT_FALSE(c2 > c1);
  ++c1;
  ASSERT_TRUE(c1 > c2);
  ASSERT_FALSE(c2 > c1);
}

TEST(test_infinite_counter_with_both_wrapping) {
  cxxpool::detail::infinite_counter<int, 2> c1;
  cxxpool::detail::infinite_counter<int, 2> c2;
  ++c1; ++c2;
  ++c1; ++c2;
  ASSERT_FALSE(c1 > c2);
  ASSERT_FALSE(c2 > c1);
  ++c1; ++c2;
  ASSERT_FALSE(c1 > c2);
  ASSERT_FALSE(c2 > c1);
  ++c1;
  ASSERT_TRUE(c1 > c2);
  ASSERT_FALSE(c2 > c1);
}

TEST(test_infinite_counter_with_one_wrapping) {
  cxxpool::detail::infinite_counter<int, 2> c1;
  cxxpool::detail::infinite_counter<int, 2> c2;
  ++c1; ++c2;
  ++c1;
  ++c1;
  ASSERT_TRUE(c1 > c2);
  ASSERT_FALSE(c2 > c1);
}

TEST(test_priority_task_noarg_construction) {
  cxxpool::detail::priority_task t1;
  cxxpool::detail::priority_task t2;
  ASSERT_FALSE(t1 < t2);
  ASSERT_FALSE(t2 < t1);
}

void some_function() {}

void some_other_function() {}

TEST(test_priority_task_with_different_priorities) {
  cxxpool::detail::infinite_counter<typename cxxpool::detail::priority_task::counter_elem_t> c;
  cxxpool::detail::priority_task t1(some_function, 3, c);
  ++c;
  cxxpool::detail::priority_task t2(some_function, 2, c);
  ASSERT_TRUE(t2 < t1);
  ASSERT_FALSE(t1 < t2);
}

TEST(test_priority_task_with_same_priorities) {
  cxxpool::detail::infinite_counter<typename cxxpool::detail::priority_task::counter_elem_t> c;
  cxxpool::detail::priority_task t1(some_function, 2, c);
  ++c;
  cxxpool::detail::priority_task t2(some_other_function, 2, c);
  ASSERT_TRUE(t2 < t1);
  ASSERT_FALSE(t1 < t2);
}

TEST(test_priority_task_with_same_priorities_and_same_order) {
  cxxpool::detail::infinite_counter<typename cxxpool::detail::priority_task::counter_elem_t> c;
  cxxpool::detail::priority_task t1(some_function, 2, c);
  cxxpool::detail::priority_task t2(some_function, 2, c);
  ASSERT_FALSE(t2 < t1);
  ASSERT_FALSE(t1 < t2);
}

TEST(test_result_get_void_empty) {
  std::vector<std::future<void>> futures;
  cxxpool::get(futures.begin(), futures.end());
}

TEST(test_result_get_void) {
  cxxpool::thread_pool pool{4};
  int a = 0;
  int b = 0;
  std::vector<std::future<void>> futures;
  futures.emplace_back(pool.push([&a]{ a = 1; }));
  futures.emplace_back(pool.push([&b]{ b = 2; }));
  cxxpool::get(futures.begin(), futures.end());
  ASSERT_EQUAL(1, a);
  ASSERT_EQUAL(2, b);
}

TEST(test_result_get_int_empty) {
  std::vector<std::future<int>> futures;
  const auto result = cxxpool::get(futures.begin(), futures.end());
  ASSERT_TRUE(result.empty());
}

TEST(test_result_get_int) {
  cxxpool::thread_pool pool{4};
  std::vector<std::future<int>> futures;
  futures.emplace_back(pool.push([]{ return 1; }));
  futures.emplace_back(pool.push([]{ return 2; }));
  const auto result = cxxpool::get(futures.begin(), futures.end());
  ASSERT_EQUAL(2u, result.size());
  ASSERT_EQUAL(1, result[0]);
  ASSERT_EQUAL(2, result[1]);
}

TEST(test_result_get_int_list) {
  cxxpool::thread_pool pool{4};
  std::vector<std::future<int>> futures;
  futures.emplace_back(pool.push([]{ return 1; }));
  futures.emplace_back(pool.push([]{ return 2; }));
  auto result = cxxpool::get(futures.begin(), futures.end(), std::list<int>{});
  ASSERT_EQUAL(2u, result.size());
  auto it = result.begin();
  ASSERT_EQUAL(1, *it);
  ++it;
  ASSERT_EQUAL(2, *it);
}

TEST(test_wait) {
  cxxpool::thread_pool pool{4};
  int a = 0;
  std::vector<std::future<void>> futures;
  futures.emplace_back(pool.push([&a]{
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    a = 1;
  }));
  cxxpool::wait(futures.begin(), futures.end());
  ASSERT_EQUAL(1, a);
}

TEST(test_wait_for) {
  condvar cv;
  cxxpool::thread_pool pool{4};
  int a = 0;
  std::vector<std::future<void>> futures;
  futures.emplace_back(pool.push([&a,&cv]{
    cv.wait();
    a = 1;
  }));
  cxxpool::wait_for(futures.begin(), futures.end(),
                    std::chrono::milliseconds(10));
  ASSERT_EQUAL(0, a);
  cv.notify_one();
  cxxpool::wait_for(futures.begin(), futures.end(),
                    std::chrono::milliseconds(50));
  ASSERT_EQUAL(1, a);
}

TEST(test_wait_until) {
  condvar cv;
  cxxpool::thread_pool pool{4};
  int a = 0;
  std::vector<std::future<void>> futures;
  futures.emplace_back(pool.push([&a,&cv]{
    cv.wait();
    a = 1;
  }));
  const auto now = std::chrono::steady_clock::now();
  cxxpool::wait_until(futures.begin(), futures.end(),
                      now + std::chrono::milliseconds(10));
  ASSERT_EQUAL(0, a);
  cv.notify_one();
  cxxpool::wait_until(futures.begin(), futures.end(),
                      now + std::chrono::milliseconds(50));
  ASSERT_EQUAL(1, a);
}

TEST(test_thread_pool_parallel_pushes) {
  cxxpool::thread_pool pool{4};
  for (size_t i=0; i < 1000; ++i) {
    auto t1 = std::thread([&pool]() { pool.push([]{}); });
    auto t2 = std::thread([&pool]() { pool.push([]{}); });
    t1.join();
    t2.join();
  }
}

TEST(test_thread_pool_add_threads) {
  cxxpool::thread_pool pool{4};
  ASSERT_EQUAL(4u, pool.n_threads());
  pool.add_threads(0);
  ASSERT_EQUAL(4u, pool.n_threads());
  pool.add_threads(2);
  ASSERT_EQUAL(6u, pool.n_threads());
}

TEST(test_thread_pool_parallel_add_threads_and_n_threads) {
  cxxpool::thread_pool pool{4};
  for (size_t i=0; i < 20; ++i) {
    auto t1 = std::thread([&pool]() { pool.add_threads(2); });
    auto t2 = std::thread([&pool]() { ASSERT_GREATER_EQUAL(pool.n_threads(), 4u); });
    t1.join();
    t2.join();
  }
}

TEST(test_thread_pool_n_tasks) {
  cxxpool::thread_pool pool;
  ASSERT_EQUAL(0u, pool.n_tasks());
  pool.push([]{ return 1; });
  pool.push([]{ return 2.; });
  ASSERT_EQUAL(2u, pool.n_tasks());
}

TEST(test_pause_and_resume) {
  cxxpool::thread_pool pool;
  auto future1 = pool.push([]{ return 1; });
  auto future2 = pool.push([]{ return 2.; });
  pool.set_pause(true);
  pool.add_threads(4);
  pool.set_pause(false);
  ASSERT_EQUAL(1, future1.get());
  ASSERT_EQUAL(2., future2.get());
}

TEST(test_clear) {
  cxxpool::thread_pool pool;
  pool.push([]{ return 1; });
  pool.push([]{ return 2.; });
  pool.clear();
  ASSERT_EQUAL(0u, pool.n_tasks());
}

COLLECTION(test_examples) {

TEST(basic_with_three_tasks) {
    std::ostringstream os;
    examples::basic_with_three_tasks(os);
    const std::string expected = "results = 42, 13, 13\n";
    ASSERT_EQUAL(expected, os.str());
}

} // test_examples
} // test_transwarp
