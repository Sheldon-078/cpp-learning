# C++ Thread Pool

一个从零实现的 C++17 线程池，用于学习现代 C++、并发原语、任务调度和性能分析。
该项目已从一个简单的全局任务队列演进为一个任务运行时，支持异步结果、有界队列、仅可移动任务，以及基于互斥锁的工作窃取。

---

## Features

固定大小的工作线程池

泛型 submit(F&&, Args&&...)

通过 std::future 返回结果

通过 std::packaged_task 传播异常

优雅关闭

有界任务队列与生产者背压

仅可移动任务包装器（MoveOnlyFunction）

每个工作线程的本地任务队列

基于互斥锁的工作窃取

基准测试与运行时插桩

---

## Project Evolution

### V0.1 - Basic Thread Pool

已实现：

固定数量的工作线程

全局任务队列

std::mutex + std::condition_variable

FIFO 任务执行

优雅关闭

基本工作流程：

等待任务
    ↓
锁定队列
    ↓
弹出任务
    ↓
解锁队列
    ↓
执行任务

#### V0.2 - Generic Async Tasks
新增支持：
auto future = pool.submit(func, args...);
实现使用：
可调用对象 + 参数
↓
std::bind
↓
std::packaged_task<R()>
↓
std::future<R>
这允许：

任意返回类型

异步结果获取

异常从工作线程传播到调用者

#### V0.3 - Bounded Queue
新增可配置的最大排队任务数。
使用两个条件变量：
not_empty
→ 工作线程等待可用任务

not_full
→ 生产者等待可用队列容量
关闭条件是：
stop == true && queued_tasks == 0
这允许已提交任务在工作线程退出前完成。

#### V0.4 - Move-Only Task Wrapper
原始实现将任务存储为：
packaged_task
↓
shared_ptr
↓
lambda
↓
std::function<void()>
因为 std::packaged_task 是仅可移动的，而 std::function 在 C++17 中要求目标可拷贝构造，所以需要额外的 shared_ptr 适配层。
为了移除该适配层，项目实现了自定义 MoveOnlyFunction。
架构：
MoveOnlyFunction
↓
unique_ptr<CallableBase>
↓
CallableHolder<T>
↓
actual callable
MoveOnlyFunction 在保留仅可移动语义的同时提供类型擦除。
任务路径变为：
callable
↓
packaged_task
↓
MoveOnlyFunction
↓
task queue

#### Work-Stealing Scheduler
当前实现用每个工作线程的队列替代了单个全局任务队列。
worker 0 <-> queue 0
worker 1 <-> queue 1
worker 2 <-> queue 2
...
每个 WorkerQueue 包含：
std::deque<MoveOnlyFunction> tasks;
std::mutex mutex;
所有者工作线程通常从尾部移除任务：
owner → pop_back()
空闲工作线程从头部窃取较旧任务：
thief → pop_front()
这给出基本结构：
front back

[ old ][ old ][ ... ][ new ][ new ]
↑ ↑
thief owner
pop_front() pop_back()
当前实现基于互斥锁，而不是无锁。

同步设计
使用两类互斥锁。
WorkerQueue 互斥锁
每个本地队列拥有自己的互斥锁。
它保护：
WorkerQueue::tasks
ThreadPool 状态互斥锁
全局 state_mutex 保护池级状态：
queued_tasks
stop
backpressure state
condition-variable predicates
当前锁顺序规则是：
state_mutex → WorkerQueue mutex
工作线程代码避免在持有 WorkerQueue 互斥锁的同时获取 state_mutex，从而防止锁顺序反转。

#### Work-Stealing Instrumentation
为了理解调度行为，添加了以下计数器：
local_pop_count
steal_count
steal_attempt_count
task_fail_count
它们测量：
local_pop_count
→ 从工作线程自己的队列执行的任务

steal_count
→ 从另一个队列成功窃取的任务

steal_attempt_count
→ 检查外部队列的尝试次数

task_fail_count
→ 未找到任务的获取尝试次数
对于每个完成的基准测试：
local_pop_count + steal_count == task_count
这也被用作任务获取的健全性检查。

#### Performance Investigation
基准工作负载：
task_count × loops_per_task
固定总计算量。
示例配置：
100 × 1,000,000
1000 × 100,000
10000 × 10,000
粗粒度任务随工作线程增加而合理扩展。
然而，细粒度任务显示出显著性能退化和不稳定性。
插桩显示了两种不同的调度模式。
本地主导模式
示例：
local tasks ≈ 97%
stolen tasks ≈ 3%
该模式表现出显著更好的性能。
窃取主导模式
一些运行显示：
stolen tasks ≈ 80% - 90%
同时伴随数万次外部队列探测。
这些运行显著更慢。
这表明过度窃取与细粒度性能差之间存在强相关性，但其本身并不能证明窃取是唯一瓶颈。

#### Submission / Execution Timing Experiment
进一步实验延迟了工作线程执行，直到所有任务首先被提交到本地队列。
在工作线程开始之前：
queue0 ≈ N / workers
queue1 ≈ N / workers
queue2 ≈ N / workers
...
在这种设置下，任务执行变得持续本地主导：
local execution ≈ 96% - 98%
并且对于测试工作负载，8 个工作线程再次优于 4 个工作线程。
这提供了证据，表明并发外部提交和消费可能导致当前调度器进入窃取密集型执行模式。
该结果并不意味着之前所有性能退化都能完全由工作窃取解释。
特别是，先前的全局队列实现也表现出细粒度扩展问题，将单独调查。

#### Known Limitations
当前调度器有意保持实验性。
重要限制包括：

外部任务被直接分配到工作线程本地队列

notify_one() 与接收任务的队列没有亲和性

在任务入队/出队记账期间仍会触碰全局 state_mutex

工作窃取当前会扫描多个受害者队列

实现基于互斥锁，而不是无锁

基准插桩本身可能引入同步开销

全局队列细粒度性能退化尚未完全解释

#### Next Steps
重新审视全局队列实现，并隔离 8 工作线程细粒度扩展差的原因。

测量基准测试中共享完成原子变量的影响。

分析队列互斥锁争用和其他同步热点。

改进外部任务注入策略。

比较替代工作窃取策略。

添加压力测试和基于 sanitizer 的并发验证。

使用 perf 等工具进行性能分析。

#### Build
mkdir -p build
cd build
cmake ..
cmake --build .
run：
./thread_pool_demo

#### Environment
- C++17
- CMake
- Linux / WSL
- GCC