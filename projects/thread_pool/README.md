# Thread Pool

## V0.2 Features
通用任务提交
支持任意可调用对象的返回类型
支持可调用对象的参数
通过 std::future 返回任务结果
通过 std::future::get() 传播任务异常
任务抛出异常后保持工作线程存活
## Design
提交流程大致如下：

可调用对象 + 参数
→ 绑定为零参数可调用对象
→ std::packaged_task<ReturnType()>
→ std::future<ReturnType>
→ std::shared_ptr
→ 包装为 std::function<void()>
→ 任务队列
→ 工作线程执行
### Why `std::shared_ptr`?
std::packaged_task 是只可移动的，而 C++17 中的 std::function 要求其目标对象必须可拷贝构造。

因此用 std::shared_ptr 来管理 packaged task，队列中存储的则是按值捕获该共享指针的可拷贝 lambda。

这也保证了 packaged task 在 submit() 返回之后依然存活。
### Exception propagation
用户任务抛出的异常会被 std::packaged_task 捕获并存储在共享状态中。