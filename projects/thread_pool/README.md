# Thread Pool

## V0.4 Features
设计benchmark
- 输入worker_count task_count loops_per_task
- 构建线程池
- 计时开始
- 提交任务，执行的任务避免被编译器优化，后续会调用每次循环的结果，当最后一个任务完成时done准备好future，同时计时结束
- 计算 elapsed throughput checksum并输出
- 返回结果
### Environment
- Intel i7-10870H
- 8 physical cores / 16 logical CPUs
- WSL2
- C++17
### Worker Scaling
固定任务数量和任务大小，只递增 worker 数（线程数），看性能如何变化
总共测试了1,2,4,8个worker数的情况，证明提升效率并不是随着worker数线性提升的
### Task Granularity
将总工作量始终保持约 1 亿次循环，修改任务粒度。分别用以下三种测试：
100 tasks    × 1,000,000 loops
1000 tasks   ×   100,000 loops
10000 tasks  ×    10,000 loops
### Findings
- 重型任务能够很好地随工作线程数量的增加而扩展。
- 细粒度任务会因调度与同步开销而性能受损。
- 增加工作线程数并不总能提升吞吐量。
- 任务粒度是 ThreadPool 设计中需要重点考虑的因素。
### MoveOnlyFunction
使用自定义 move-only type-erased callable，允许 packaged_task 直接进入任务队列，移除了 shared_ptr + lambda + std::function 适配层。A/B benchmark 显示该改造没有消除细粒度任务下 8-worker 的性能退化，因此下一步将关注全局任务队列与同步架构。
