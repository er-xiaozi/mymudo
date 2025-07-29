#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0 )
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking())
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);

    //baseLoop有客户端连接 调用回调 =》connfd=>channel=>subloop
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}
Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();// acceptChannel注册到Poller中
}

// listenfd有新用户连接事件发生时执行
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >=0 )
    {
        if (newConnectionCallback_)
        {
            newConnectionCallback_(connfd, peerAddr);   //轮询找到subloop,唤醒，分发当前新客户端的channel
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d  accept err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno = EMFILE) // 达到当前进程可以打开文件描述符的上限，没有可用fd分配了
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit! \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}
