#include "Channel.hpp"
#include <sys/epoll.h>
#include "EventLoop.hpp" 
#include "Logger.hpp"

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

//EventLoop: ChannelList Poller
Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
    {}

void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true; 
}

/*
* 当改变channel所表示fd的events事件后，update负责在poller里面更改fd相应的事件epoll_ctl
* EventLoop => ChannelList Poller
*/
void Channel::update()
{
    // 通过channel所属的EventLoop，调用poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}

//在channel所属的EventLoop中，把当前的channel删除掉
void Channel::remove()
{
    loop_->removeChannel(this);
}

//fd得到poller通知以后，处理事件的
void Channel::handleEvent(Timestamp receiveTime)
{
    if(tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if(guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

//根据Channel发生的具体时间，由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent revents:%d\n", revents_);

    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if(closecallback_)
        {
            closecallback_();
        }
    }

    if(revents_ & EPOLLERR)
    {
        if(errorcallback_)
        {
            errorcallback_();
        }
    }

    if(revents_ & (EPOLLIN | EPOLLPRI))
    {
        if(readcallback_)
        {
            readcallback_(receiveTime);
        }
    }

    if(revents_ & EPOLLOUT)
    {
        if(writecallback_)
        {
            writecallback_();
        }
    }
}