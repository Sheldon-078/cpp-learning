#pragma once

#include <memory>
#include <type_traits>
#include <utility>
#include <functional>

struct CallableBase
{
    virtual void call() = 0;
    virtual ~CallableBase() = default;
};

template <typename F>
struct CallableHolder : CallableBase
{
    F f;
    template <typename U>
    CallableHolder(U &&func) : f(std::forward<U>(func)) {};
    void call() override
    {
        f();
    }
};

class MoveOnlyFunction
{
private:
    std::unique_ptr<CallableBase> ptr;

public:
    template <
        typename F,
        typename = std::enable_if_t<
            !std::is_same_v<std::decay_t<F>, MoveOnlyFunction>>>
    MoveOnlyFunction(F &&f)
    {
        using T = std::decay_t<F>;
        ptr = std::make_unique<CallableHolder<T>>(std::forward<F>(f));
    }
    void operator()()
    {
        if (!ptr)
            throw std::bad_function_call();
        ptr->call();
    }
    MoveOnlyFunction() = default;
    MoveOnlyFunction(const MoveOnlyFunction &) = delete;
    MoveOnlyFunction &operator=(const MoveOnlyFunction &) = delete;
    MoveOnlyFunction(MoveOnlyFunction &&)noexcept = default;
    MoveOnlyFunction &operator=(MoveOnlyFunction &&) noexcept= default;
};
