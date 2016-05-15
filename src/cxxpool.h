#pragma once
#include <thread>
#include <future>
#include <stdexcept>
#include <queue>
#include <utility>
#include <list>
#include <functional>
#include <memory>
#include <cstddef>
#include <vector>


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
  auto push(int priority, Functor&& functor, Args&&... args)
    -> std::future<typename std::result_of<Functor(Args...)>::type> {
    if (priority < 0)
      throw thread_pool_error{"priority cannot be smaller than zero"};
    using result_type = typename std::result_of<Functor(Args...)>::type;
    auto pack_task = std::make_shared<std::packaged_task<result_type()>>(
      std::bind(std::forward<Functor>(functor), std::forward<Args>(args)...));
    auto future = pack_task->get_future();
    {
        std::lock_guard<std::mutex> lock{mutex_};
        tasks_.emplace([pack_task]{ (*pack_task)(); }, priority);
    }
    cond_var_.notify_one();
    return future;
  }

  template<typename Functor, typename... Args>
  auto push(Functor&& functor, Args&&... args)
    -> std::future<typename std::result_of<Functor(Args...)>::type> {
    return push(0, std::forward<Functor>(functor), std::forward<Args>(args)...);
  }

 private:

  void start_threads() {
    threads_ = std::list<std::thread>(max_n_threads_);
    for (auto& thread : threads_)
      thread = std::thread{&thread_pool::run, this};
  }

  void run() {
    for (;;) {
      priority_task task;
      {
        std::unique_lock<std::mutex> lock{mutex_};
        cond_var_.wait(lock, [this]{ return done_ || !tasks_.empty(); });
        if (done_ && tasks_.empty())
            break;
        task = tasks_.top();
        tasks_.pop();
      }
      task.callback();
    }
  }

  int hardware_concurrency() const noexcept {
    const auto n_threads = std::thread::hardware_concurrency();
    if (n_threads == 0)
      throw thread_pool_error{
      "invalid value from std::thread::hardware_concurrency()"};
    return static_cast<int>(n_threads);
  }

  struct priority_task {

    static std::vector<std::uint64_t> task_counter_;

    priority_task()
    : callback{}, priority{}, order{}
    {}

    priority_task(std::function<void()> callback, int priority)
    : callback{std::move(callback)}, priority{priority}, order{}
    {
      constexpr auto max = std::numeric_limits<
          typename decltype(task_counter_)::value_type>::max();
      if (task_counter_.empty() || task_counter_.back() == max)
        task_counter_.push_back(0);
      else
        ++task_counter_.back();
      order = task_counter_;
    }

    std::function<void()> callback;
    int priority;
    decltype(task_counter_) order;

    bool operator<(const priority_task& other) const {
      if (priority == other.priority) {
        if (order.size() == other.order.size()) {
          return order.back() > other.order.back();
        } else {
          return order.size() > other.order.size();
        }
      } else {
        return priority < other.priority;
      }
    }
  };

  std::list<std::thread> threads_;
  bool done_;
  int max_n_threads_;
  std::priority_queue<priority_task, std::deque<priority_task>> tasks_;
  std::condition_variable cond_var_;
  std::mutex mutex_;
};

std::vector<std::uint64_t> thread_pool::priority_task::task_counter_{};


}  // namespace cxxpool
