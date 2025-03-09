#include "TcpConnection.hpp"
#include "Logger.hpp"
#include "Channel.hpp"
#include "EventLoop.hpp"
#include "Socket.hpp"
#include <functional>


static EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainloop is null! \n", __FILE__, __FUNCTION__, __LINE__);  
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, 
    const std::string &nameArg,
    int sockfd,
    const InetAddress &localAddr,
    const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64*1024*1024)
    {
        channel_->setReadCallback(
            std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
        channel_->setWriteCallback(
            std::bind(&TcpConnection::handleWrite, this));
        channel_->setCloseCallback(
            std::bind(&TcpConnection::handleClose, this));
        channel_->setErrorCallback(
            std::bind(&TcpConnection::handleError, this));

        LOG_INFO("TcpConnection::ctor[%s] at fd=%d \n", name_.c_str(), sockfd);
        socket_->setKeepAlive(true);
    }

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n", name_.c_str(), channel_->fd(), int(state_));
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int saveErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
    if(n > 0)
    {
        //已建立连接的用户，有可读事件发生了，调用用户传入的回调操作onMessage
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if(n == 0)
    {
        handleClose();
    }
    else
    {
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if(channel_->isWriting())
    {
        int saveErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
        if(n > 0)
        {
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if(writeCompleteCallback_)
                {
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this()));
                    if(state_ == kDisconnecting)
                    {
                        shutdownInloop();
                    }
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());
    }
}

void TcpConnection::handleClose()
{
    LOG_INFO("fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);
    closeCallback_(connPtr);
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;
    if(getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}

void TcpConnection::send(const std::string &buf)
{
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,
                this,
                buf.c_str(),
                buf.size()
            ));  
        }
    }
}

/*
*发送数据 应用写的快，而内核发送数据慢，需要把待发送数据写入缓冲区，而且设置了水位回调
*/
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    //之前调用过该connection的shutdown，不能再进行发送了
    if(state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }

    //表示channel_第一次开始写数据，而且缓冲区没有待发送数据
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = write(channel_->fd(), data, len);
        if(nwrote >= 0)
        {
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_)
            {
                //既然在这里数据全部发送完成，就不用再给channel_设置epollout事件了
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, shared_from_this())
                );
            }
        }
        else
        {
            nwrote = 0;
            if(errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if(errno == EPIPE || errno ==  ECONNRESET)
                {
                    faultError = true;
                }
            }
        }
    }

    //说明当前这一次write，并没有把数据完全发送出去，剩余的数据需要保存到缓冲区当中
    //然后给channel注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知相应的sock-channel,
    //调用writeCallback_回调方法，也就是调用TcpConnection::handleWrite方法，把发送缓冲区中的数据全部发送完成
    if(!faultError && remaining > 0)
    {
        size_t oldlen = outputBuffer_.readableBytes();
        if(oldlen + remaining >= highWaterMark_
            && oldlen < highWaterMark_
            && highWaterCallback_)
        {
            loop_->queueInLoop(std::bind(
                highWaterCallback_, shared_from_this(), oldlen + remaining
            ));
        }
        outputBuffer_.append((char*)data + nwrote, remaining);
        if(!channel_->isWriting())
        {
            channel_->enableWriting();
        }
    }

}

//连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    //新连接建立，执行回调
    connectionCallback_(shared_from_this());
}

//连接销毁
void TcpConnection::connectDestoryed()
{
    if(state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll(); 
        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}

//关闭连接
void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(
            &TcpConnection::shutdownInloop, this
        ));
    }
}

void TcpConnection::shutdownInloop()
{
    //说明outputBuffer中的数据已经全部发送完成
    if(!channel_->isWriting())
    {
        socket_->shutdownWrite();
    }
}