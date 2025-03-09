#pragma once
#include <functional>
#include "noncopyable.hpp"
#include <memory>
#include "Timestamp.hpp"

class EventLoop;
class Timestamp;

/*
* Channel 理解为通道，封装了sockfd 和其感兴趣的event,如EPOLLIN、EPOLLOUT事件
* 还绑定了poller返回的具体事件
*/
class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel() = default;

    //fd得到poller通知以后，处理事件的
    void handleEvent(Timestamp receiveTime);

    //设置回调函数对象
    void setReadCallback(ReadEventCallback cb)
    {  readcallback_ = std::move(cb);  }

    void setWriteCallback(EventCallback cb)
    {  writecallback_ = std::move(cb);  }

    void setCloseCallback(EventCallback cb)
    {  closecallback_ = std::move(cb);  }

    void setErrorCallback(EventCallback cb)
    {  errorcallback_ = std::move(cb);  }

    //防止当Channel被手动remove掉，channel还在执行回调操作
    void tie(const std::shared_ptr<void>&);

    int fd() const { return fd_; }

    int events() const { return events_; }

    void set_revents(int revt) { this->revents_ = revt; }

    //设置fd相应的事件状态
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kReadEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }
    
    //返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() const { return index_; }
    void set_index(int idx) { index_ = idx; }

    //one loop per thread
    EventLoop* ownerLoop() { return loop_; }
    void remove();

private:

    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_; //事件循环
    const int fd_; //fd,Poller监听的对象
    int events_; //注册fd感兴趣的事件
    int revents_; //poller返回的具体发生的事件
    int index_;

    std::weak_ptr<void> tie_;
    bool tied_;

    //因为Channel通道里面能够获知fd最终发生的具体的事件revents，所以它负责调用具体事件的回调操作
    ReadEventCallback readcallback_;
    EventCallback writecallback_;
    EventCallback closecallback_;
    EventCallback errorcallback_;
};

