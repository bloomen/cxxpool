#pragma once
#include <thread>
#include <future>
#include <stdexcept>
#include <queue>
#include <utility>
#include <list>
#include <functional>
#include <memory>


namespace cxxpool {


class thread_pool_error : public std::runtime_error {
  using std::runtime_error::runtime_error;
};


class thread_pool {
 public:

  explicit thread_pool(int max_n_threads)
  : threads_{}, done_{false}, max_n_threads_{max_n_threads}, tasks_{},
    cond_var_{}, mutex_{}
  {
    if (max_n_threads_ <= 0)
      max_n_threads_ = hardware_concurrency();
    start_threads();
  }

  thread_pool()
  : thread_pool{0}
  {}

  thread_pool(const thread_pool&) = delete;
  thread_pool& operator=(const thread_pool&) = delete;
  thread_pool(thread_pool&&) = delete;
  thread_pool& operator=(thread_pool&&) = delete;

  ~thread_pool() {
    {
      std::lock_guard<std::mutex> lock{mutex_};
      done_ = true;
    }
    cond_var_.notify_all();
    for (auto& thread : threads_)
      thread.join();
  }

  int max_n_threads() const noexcept {
    return max_n_threads_;
  }

  template<typename Functor, typename... Args>
  auto push(Functor&& functor, Args&&... args)
    -> std::future<typename std::result_of<Functor(Args...)>::type> {
    using result_type = typename std::result_of<Functor(Args...)>::type;
    auto pack_task = std::make_shared<std::packaged_task<result_type()>>(
      std::bind(std::forward<Functor>(functor), std::forward<Args>(args)...));
    auto future = pack_task->get_future();
    {
        std::lock_guard<std::mutex> lock{mutex_};
        tasks_.emplace([pack_task]{ (*pack_task)(); });
    }
    cond_var_.notify_one();
    return future;
  }

 private:

  void start_threads() {
    threads_ = std::list<std::thread>(max_n_threads_);
    for (auto& thread : threads_)
      thread = std::thread{&thread_pool::run, this};
  }

  void run() {
    for (;;) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock{mutex_};
        cond_var_.wait(lock, [this]{ return done_ || !tasks_.empty(); });
        if (done_ && tasks_.empty())
            break;
        task = std::move(tasks_.front());
        tasks_.pop();
      }
      task();
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
  bool done_;
  int max_n_threads_;
  std::queue<std::function<void()>> tasks_;
  std::condition_variable cond_var_;
  std::mutex mutex_;
};


}  // namespace cxxpool
