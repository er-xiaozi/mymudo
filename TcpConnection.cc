#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>

#include <functional>
#include <errno.h>

static EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                const std::string &nameArg,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr)
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64*1024*1024)  //64M
{
    // 设置channel相应的回调函数
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1)
    );
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this)
    );
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this)
    );
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this)
    );

    LOG_INFO("TcpConnsection::ctor[%s] sat fd=%d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n", 
        name_.c_str(), channel_->fd(), (int)state_);
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

// 应用写的块，内核发送慢，需要把待发送的数据写入缓冲区，设置水位回调
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    if(state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }
    // channel第一次开始写数据，或者缓冲区没有待发送数据
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0) //检查当前 Channel 是否已经注册了 EPOLLOUT 写事件
    {
        nwrote = ::write(channel_->fd(), data, len);
        if(nwrote > 0)
        {
            remaining = len - nwrote;
            if (remaining = 0 && writeCompleteCallback_)
            {
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
                LOG_ERROR("TcpConnection::snedInLoop");
                if(errno == EPIPE || errno == ECONNRESET)   // 收到连接重置请求
                {
                    faultError = true;
                }
            }
        }
    }
    // 说明当前这一次write, 并没有把数据全部发送出去， 剩余的数据需要保存到缓冲区中， 然后给channnel
    // 注册epollout事件, poller发现tcp的发送缓冲区有空间，会通知相应的sock-channel, 调用handleWrite方法
    // 调用TcpConnection::handlewrite方法，把发送缓冲区的数据全部发送完成 
    /*
    * 如果数据没有全部写入（remaining > 0），并且没有发生错误（faultError == false），
    * 则需要将剩余数据缓冲到 outputBuffer_ 中，并注册 EPOLLOUT 事件，等待 socket 可写时继续发送。
    */
    if (!faultError && remaining > 0)
    {
        //  目前发送缓冲区剩余待发送数据的长度
        size_t oldlen = outputBuffer_.readableBytes();
        if (oldlen + remaining >= highWaterMark_ 
            && oldlen < highWaterMark_ 
            && highWaterMarkCallback_)
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldlen + remaining)
            );
        }
        outputBuffer_.append((char*)data + nwrote, remaining);
        if(!channel_->isWriting())
        {
            channel_->enableWriting();  // 注册channel的写事件
        }
    }
}

// 连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();  // 向poller注册channel的epollin事件

    // 新连接建立， 执行回调
    connectionCallback_(shared_from_this());
}
// 连接销毁
void TcpConnection::connectDestroyed()
{
    if(state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll(); // 把channel所有感兴趣的事件，从poller中删除
        connectionCallback_(shared_from_this());
    }
    channel_->remove(); //把channel从poller中删除
}
void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setState(kDisconnected);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}
void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting())  // 如果outputBuffer_中的数据已经发送完成
    {
        socket_->shutdownWrite();//触发EPOLLHUB事件（不用注册）
    }
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if(n > 0)
    {
        // 已建立连接的用户， 有可读事件发生了， 调用用户传入的回调操作onMessage
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if ( n == 0 )
    {
        handleClose();
    }
    else
    {
        errno  = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}
void TcpConnection::handleWrite()
{
    if(channel_->isWriting()) // 写事件状态
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if(n > 0)
        {
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes() == 0) //发送完成
            {
                channel_->disableWriting();
                if(writeCompleteCallback_)
                {
                    loop_->queueInLoop( //TcpConnection 是在mainLoop创建的，相关回调在subloop执行
                        std::bind(writeCompleteCallback_, shared_from_this())
                    );
                }
                if(state_ == kDisconnecting)
                {
                    shutdownInLoop();   // 在当前Loop中删除TcpConnection
                }
            }
            else
            {
                LOG_ERROR("Tcp::Connection::handleWrite");
            }
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());
    }
}
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);   // 执行关闭连接的回调
    closeCallback_(connPtr);    // 关闭连接的回调
}
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}