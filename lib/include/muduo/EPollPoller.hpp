#pragma once
#include "Poller.hpp"
#include <sys/epoll.h>
#include <vector>

class EPollPoller : public Poller
{    
public:

    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    //重写基类Poller的抽象方法
    Timestamp poll(const int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel *channel) override;

private:

    static const int kInitEventListSize = 16;

    //填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    //更新channel通道
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};


