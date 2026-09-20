#include"worker_queue.h"


std::optional<MoveOnlyFunction> WorkerQueue::try_pop()
{
    std::lock_guard<std::mutex> lock(mutex);

    if (tasks.empty())
        return std::nullopt;

    MoveOnlyFunction task = std::move(tasks.back());
    tasks.pop_back();

    return std::optional<MoveOnlyFunction>(
        std::move(task));
}
std::optional<MoveOnlyFunction> WorkerQueue::try_steal()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (tasks.empty())
    {
        return std::nullopt;
    }
    MoveOnlyFunction task = std::move(tasks.front());
    tasks.pop_front();
    return std::optional<MoveOnlyFunction>(
        std::move(task));
}
void WorkerQueue::push(MoveOnlyFunction &&task)
{
    std::lock_guard<std::mutex> lock(mutex);
    tasks.push_back(std::move(task));
}
