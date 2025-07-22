#include "TcpServer.h"
#include "Logger.h"

#include <functional>

EventLoop* ChecLoopNotNull(EventLoop *loop)
{
    if (loop = nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}
    

TcpServer::TcpServer(EventLoop *loop,
    const InetAddress &listenAddr,
    const std::string &nameArg,
    Option option)
    : loop_(ChecLoopNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option==kNoReusePort))
    , threadPool_(new EventLoopThreadPoll(loop, name_))
    , connectionCallback_()
    , messageCallback_()
    , nextConnId_(1)
{
    // 当有新用户连接时会执行TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
         std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
}


void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

// 开启服务器监听
void TcpServer::start()
{
    if (started_ ++ ==0)    //防止一个Tcpserver对象被start多次
    {
        threadPool_->start(threadInitCallback_);    //启动loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    
}
