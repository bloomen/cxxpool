#pragma once
#include <thread>
#include <future>
#include <stdexcept>
#include <queue>
#include <utility>
#include <list>


namespace cxxpool {


class thread_pool_error : public std::runtime_error {
  using std::runtime_error::runtime_error;
};


template<typename ResultType>
class thread_pool {
 public:

  explicit thread_pool(int max_n_threads)
  : threads_{}, done_{false}, max_n_threads_{max_n_threads}, tasks_{}
  {
    if (max_n_threads_ <= 0) {
      max_n_threads_ = hardware_concurrency();
    }
  }

  thread_pool()
  : thread_pool{0}
  {}

  ~thread_pool() {
    done_ = true;
    for (auto& thread : threads_)
      thread.join();
  }

  int max_n_threads() const noexcept {
    return max_n_threads_;
  }

  template<typename Functor>
  std::future<ResultType> push(Functor&& functor) {
    std::packaged_task<ResultType()> task{std::forward<Functor>(functor)};
    auto future = task.get_future();
    task();  // TODO(cblume): fix this
    tasks_.push(std::move(task));
    return future;
  }


 private:

  void start_threads() {
    for (int i=0; i < max_n_threads_; ++i)
      threads_.push_back(std::thread{&thread_pool::run, this});
  }

  void run() {
    while (!done_) {

    }
  }

  int hardware_concurrency() const noexcept {
    const auto n_threads = std::thread::hardware_concurrency();
    if (n_threads == 0)
      throw thread_pool_error{
      "invalid value from std::thread::hardware_concurrency()"};
    return static_cast<int>(n_threads);
  }

  std::list<std::thread> threads_;
  std::atomic_bool done_;
  int max_n_threads_;
  std::queue<std::packaged_task<ResultType()>> tasks_;
};


}  // namespace cxxpool
