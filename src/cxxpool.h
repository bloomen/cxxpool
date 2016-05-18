#pragma once
#include <thread>
#include <future>
#include <stdexcept>
#include <queue>
#include <utility>
#include <functional>
#include <memory>
#include <cstddef>
#include <vector>


namespace cxxpool {


class thread_pool_error : public std::runtime_error {
 public:
  explicit thread_pool_error(const std::string& message)
  : std::runtime_error{message} {}
};


class thread_pool {
 public:

  thread_pool();

  explicit thread_pool(int n_threads);

  thread_pool(const thread_pool&) = delete;
  thread_pool& operator=(const thread_pool&) = delete;
  thread_pool(thread_pool&&) = delete;
  thread_pool& operator=(thread_pool&&) = delete;

  ~thread_pool();

  int n_threads() const noexcept;

  template<typename Functor, typename... Args>
  auto push(Functor&& functor, Args&&... args)
    -> std::future<decltype(functor(args...))>;

  template<typename Functor, typename... Args>
  auto push(int priority, Functor&& functor, Args&&... args)
    -> std::future<decltype(functor(args...))>;

 private:

  void init(int n_threads);

  void run_tasks();

  int hardware_concurrency() const;

  struct priority_task {
    typedef std::uint64_t counter_elem_t;
    static std::vector<counter_elem_t> task_counter_;

    priority_task();
    priority_task(std::function<void()> callback, int priority);

    std::function<void()> callback;
    int priority;
    std::vector<counter_elem_t> order;

    bool operator<(const priority_task& other) const;
  };

  bool done_;
  std::vector<std::thread> threads_;
  std::priority_queue<priority_task> tasks_;
  std::condition_variable cond_var_;
  std::mutex mutex_;
};

std::vector<std::uint64_t> thread_pool::priority_task::task_counter_{};

thread_pool::thread_pool()
: done_{false}, threads_{}, tasks_{},
  cond_var_{}, mutex_{}
{
  init(0);
}

thread_pool::thread_pool(int n_threads)
: done_{false}, threads_{}, tasks_{},
  cond_var_{}, mutex_{}
{
  init(n_threads);
}

thread_pool::~thread_pool() {
  {
    std::lock_guard<std::mutex> lock{mutex_};
    done_ = true;
  }
  cond_var_.notify_all();
  for (auto& thread : threads_)
    thread.join();
}

int thread_pool::n_threads() const noexcept {
  return threads_.size();
}

template<typename Functor, typename... Args>
auto thread_pool::push(Functor&& functor, Args&&... args)
  -> std::future<decltype(functor(args...))> {
  return push(0, std::forward<Functor>(functor), std::forward<Args>(args)...);
}

template<typename Functor, typename... Args>
auto thread_pool::push(int priority, Functor&& functor, Args&&... args)
  -> std::future<decltype(functor(args...))> {
  if (priority < 0)
    throw thread_pool_error{"priority smaller than zero: " +
                            std::to_string(priority)};
  typedef decltype(functor(args...)) result_type;
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

void thread_pool::init(int n_threads) {
  if (n_threads <= 0)
    n_threads = hardware_concurrency();
  threads_ = std::vector<std::thread>(n_threads);
  for (auto& thread : threads_)
    thread = std::thread{&thread_pool::run_tasks, this};
}

void thread_pool::run_tasks() {
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

int thread_pool::hardware_concurrency() const {
  const auto n_threads = std::thread::hardware_concurrency();
  if (n_threads == 0)
    throw thread_pool_error{
    "got zero from std::thread::hardware_concurrency()"};
  return static_cast<int>(n_threads);
}

thread_pool::priority_task::priority_task()
: callback{}, priority{}, order{}
{}

thread_pool::priority_task::priority_task(
    std::function<void()> callback, int priority)
: callback{std::move(callback)}, priority{priority}, order{}
{
  if (task_counter_.empty() ||
      task_counter_.back() == std::numeric_limits<counter_elem_t>::max())
    task_counter_.push_back(0);
  else
    ++task_counter_.back();
  order = task_counter_;
}

bool thread_pool::priority_task::operator<(const priority_task& other) const {
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


}  // namespace cxxpool
