#include "EventLoopThreadPool.hpp"

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0)
    {}

EventLoopThreadPool::~EventLoopThreadPool()
    {}

void EventLoopThreadPool::start(const ThreadInitCallback &cb)
{
    started_ = true;

    for(int i = 0; i < numThreads_; i++)
    {
        char buf[sizeof(name_) + 32];
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
        EventLoopThread* elt = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(elt));
        loops_.push_back(elt->startLoop());
    }

    if(numThreads_ == 0 && cb)
    {
        cb(baseLoop_);
    }
}

//如果工作在多线程中，baseloop_默认以轮询的方式分配channel的subloop
EventLoop* EventLoopThreadPool::getNextLoop()
{
    EventLoop *loop = baseLoop_;

    if(!loops_.empty())
    {
        loop = loops_[next_];
        next_++;
        if(next_ >= loops_.size())
        {
            next_ = 0;
        }
    }
    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    if(loops_.empty())
    {
        return std::vector<EventLoop*>(1, baseLoop_);
    } 
    return loops_;
}
