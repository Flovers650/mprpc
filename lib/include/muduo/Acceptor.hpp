#pragma once
#include "Channel.hpp"
#include <functional>
#include "InetAddress.hpp"
#include "Socket.hpp"

class Acceptor
{
public:

    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnetionCallback(const NewConnectionCallback &cb)
    {
        newConnectionCallback_ = std::move(cb);
    }

    bool listening() const { return listening_; }
    void listen();
private:

    void handleRead();

    EventLoop *loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_;
};
