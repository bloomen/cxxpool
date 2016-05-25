#pragma once
#include <thread>
#include <future>
#include <stdexcept>
#include <queue>
#include <utility>
#include <functional>
#include <memory>
#include <vector>


namespace cxxpool {
/**
 * A thread pool written in C++11.
 *
 * Constructing the thread pool launches the worker threads while
 * destructing joins them. The threads will be alive for as long as the
 * thread pool is not destructed.
 *
 * Tasks can be pushed into the pool with and w/o providing a priority >= 0.
 * Not providing a priority is equivalent to providing a priority of 0.
 * Those tasks are processed first that have the highest priority.
 * If priorities are equal those tasks are processed first that were pushed
 * first into the pool (FIFO).
 */
class thread_pool {
 public:
  /**
   * Constructor. The number of threads to launch is determined by calling
   * std::thread::hardware_concurrency()
   * @throws cxxpool::thread_pool_error if hardware concurrency is invalid
   */
  thread_pool();
  /**
   * Constructor
   * @param n_threads The number of threads to launch. Passing 0 is equivalent
   *  to calling the no-argument constructor
   * @throws cxxpool::thread_pool_error if n_threads < 0
   */
  explicit thread_pool(int n_threads);
  /**
   * Destructor. Joins all threads launched in the constructor. Waits for all
   * running tasks to complete
   */
  ~thread_pool();

  thread_pool(const thread_pool&) = delete;
  thread_pool& operator=(const thread_pool&) = delete;
  thread_pool(thread_pool&&) = delete;
  thread_pool& operator=(thread_pool&&) = delete;

  /**
   * Returns the number of threads launched in the constructor
   */
  int n_threads() const;
  /**
   * Pushes a new task into the thread pool. The task will have a priority of 0
   * @param functor The functor to call
   * @param args The arguments to pass to the functor when calling it
   * @return A future associated to the underlying task
   */
  template<typename Functor, typename... Args>
  auto push(Functor&& functor, Args&&... args)
    -> std::future<decltype(functor(args...))>;
  /**
   * Pushes a new task into the thread pool while providing a priority
   * @param priority A task priority. Higher priorities are processed first
   * @param functor The functor to call
   * @param args The arguments to pass to the functor when calling it
   * @return A future associated to the underlying task
   * @throws cxxpool::thread_pool_error if priority < 0
   */
  template<typename Functor, typename... Args>
  auto push(int priority, Functor&& functor, Args&&... args)
    -> std::future<decltype(functor(args...))>;

 private:

  void init(int n_threads);

  void run_tasks();

  int hardware_concurrency() const;

  void increment_task_counter();

  typedef unsigned int counter_elem_t;

  struct priority_task {
    priority_task();
    priority_task(std::function<void()> callback, int priority,
                  const std::vector<counter_elem_t>& task_counter);

    std::function<void()> callback;
    int priority;
    std::vector<counter_elem_t> order;

    bool operator<(const priority_task& other) const;
  };

  bool done_;
  std::vector<std::thread> threads_;
  std::priority_queue<priority_task> tasks_;
  std::vector<counter_elem_t> task_counter_;
  std::condition_variable cond_var_;
  std::mutex mutex_;
};


class thread_pool_error : public std::runtime_error {
 public:
  explicit thread_pool_error(const std::string& message);
};


inline
thread_pool::thread_pool()
: done_{false}, threads_{}, tasks_{},
  task_counter_{}, cond_var_{}, mutex_{}
{
  init(0);
}

inline
thread_pool::thread_pool(int n_threads)
: done_{false}, threads_{}, tasks_{},
  task_counter_{}, cond_var_{}, mutex_{}
{
  init(n_threads);
}

inline
thread_pool::~thread_pool() {
  {
    std::lock_guard<std::mutex> lock{mutex_};
    done_ = true;
  }
  cond_var_.notify_all();
  for (auto& thread : threads_)
    thread.join();
}

inline
int thread_pool::n_threads() const {
  return threads_.size();
}

template<typename Functor, typename... Args>
inline
auto thread_pool::push(Functor&& functor, Args&&... args)
  -> std::future<decltype(functor(args...))> {
  return push(0, std::forward<Functor>(functor), std::forward<Args>(args)...);
}

template<typename Functor, typename... Args>
inline
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
      increment_task_counter();
      tasks_.emplace([pack_task]{ (*pack_task)(); }, priority, task_counter_);
  }
  cond_var_.notify_one();
  return future;
}

inline
void thread_pool::init(int n_threads) {
  if (n_threads <= 0)
    n_threads = hardware_concurrency();
  threads_ = std::vector<std::thread>(n_threads);
  for (auto& thread : threads_)
    thread = std::thread{&thread_pool::run_tasks, this};
}

inline
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

inline
int thread_pool::hardware_concurrency() const {
  const auto n_threads = std::thread::hardware_concurrency();
  if (n_threads == 0)
    throw thread_pool_error{
    "got zero from std::thread::hardware_concurrency()"};
  return static_cast<int>(n_threads);
}

inline
void thread_pool::increment_task_counter() {
  if (task_counter_.empty() ||
      task_counter_.back() == std::numeric_limits<counter_elem_t>::max())
    task_counter_.push_back(0);
  else
    ++task_counter_.back();
}


inline
thread_pool::priority_task::priority_task()
: callback{}, priority{}, order{}
{}

inline
thread_pool::priority_task::priority_task(
    std::function<void()> callback, int priority,
    const std::vector<counter_elem_t>& task_counter)
: callback{std::move(callback)}, priority{priority}, order{task_counter}
{}

inline
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


inline
thread_pool_error::thread_pool_error(const std::string& message)
: std::runtime_error{message} {}


}  // namespace cxxpool
