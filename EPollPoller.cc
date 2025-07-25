#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>//
#include <cstring>
//#include <strings.h> bzero()

const int kNew = -1;    // 表示epoll的状态  channel 未添加到poller中
const int kAdded = 1;   // 未添加
const int kDeleted = 2; // 已删除

//epoll_create
EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))  //epoll_create()
    , events_(kInitEventListSize)   //16
{
    if (epollfd_<0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}
EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // 实际上应该用LOG_DEBUG输出更合理
    LOG_INFO("func=%s => fd total count:%lu\n", __FUNCTION__, channels_.size());

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int savedErrno = errno;
    Timestamp now(Timestamp::now());

    if(numEvents > 0)
    {
        LOG_INFO("%d events happened \n", numEvents);
        // 
        fillAciveChannels(numEvents, activeChannels);
        if(numEvents == events_.size())
        {
            events_.resize(events_.size()*2);
        }
    }
    else if(numEvents == 0) //超时
    {
        LOG_INFO("%s timeout \n", __FUNCTION__);
    }
    else    // 发生错误
    {
        if(savedErrno != EINTR)
        {
            errno = savedErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}
void EPollPoller::updateChannel(Channel *channel) 
{
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);
    if(index == kNew || index == kDeleted)  // 如果未添加或已删除
    {
        if(index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    } 
    else    // 已经注册过了
    {
        int fd = channel->fd();
        if(channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 把channel从poller中删除
void EPollPoller::removeChannel(Channel *channel) 
{
    int fd = channel->fd();
    channels_.erase(fd);
    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, fd);
    int index = channel->index();
    if(index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

// 填写活跃的连接
void EPollPoller::fillAciveChannels(int numEvents, ChannelList *activeChannels) const
{
    for(int i = 0; i<numEvents; ++i)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); // EventLoop就拿到Poller返回的发生所有事件的channel列表了
    }
}

// 更新channel通道
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    //    bzero(&event, sizeof event);
    memset(&event, 0, sizeof event);
    int fd = channel->fd();
    event.data.fd =fd;
    event.events = channel->events();
    event.data.ptr = channel;
    

    if(::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if(operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctrl del erroe:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }   
    }
}