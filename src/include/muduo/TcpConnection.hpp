#pragma once
#include <memory>
#include "noncopyable.hpp"
#include <string>
#include <atomic>
#include "InetAddress.hpp"
#include "Callbacks.hpp"
#include "Buffer.hpp"

class Channel;
class EventLoop;
class Socket;

class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop, 
                const std::string &nameArg,
                int sockfd,
                const InetAddress &localAddr,
                const InetAddress &peerAddr);

    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string name() const { return name_; }
    const InetAddress& loaclAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }
    //发送信息
    void send(const std::string &buf);
    //关闭连接
    void shutdown();

    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }
    void setHighWaterCallback(const HighWaterCallback &cb, size_t highWaterMark)
    {
        highWaterCallback_ = cb;
        highWaterMark_ =highWaterMark;
    }
    void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }

    //连接建立
    void connectEstablished();
    //连接销毁
    void connectDestoryed();

private:
    enum StateE {kDisconnected, kConnecting, kConnected, kDisconnecting};

    void setState(StateE state) { state_ = state; }

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void *message, size_t len);
    void shutdownInloop();

    EventLoop *loop_;
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_; //有新连接时的回调
    MessageCallback messageCallback_; //有读写信息时的回调
    WriteCompleteCallback writeCompleteCallback_; //消息发送完成以后的回调
    CloseCallback closeCallback_;
    HighWaterCallback highWaterCallback_;

    size_t highWaterMark_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;
};