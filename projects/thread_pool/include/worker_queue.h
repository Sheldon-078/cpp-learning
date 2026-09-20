#pragma once

#include <memory>
#include <type_traits>
#include <utility>
#include <iostream>
#include <future>
#include <deque>
#include <functional>
#include <optional>
#include "move_only_function.h"

class WorkerQueue
{
private:
    std::deque<MoveOnlyFunction> tasks;
    std::mutex mutex;

public:
    std::optional<MoveOnlyFunction> try_pop();
    std::optional<MoveOnlyFunction> try_steal();
    void push(MoveOnlyFunction &&task);
};