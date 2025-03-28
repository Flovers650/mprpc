#pragma once
#include "EventLoop.hpp"
#include "Thread.hpp"
#include "noncopyable.hpp"
#include <functional>
#include <condition_variable>

class EventLoopThread : noncopyable
{
public:

    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
        const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();
private:

    void threadFunc();

    EventLoop *loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mtx_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};