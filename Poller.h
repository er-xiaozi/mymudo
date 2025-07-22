#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>
// #include "Channel.h"
class Channel;
class EventLoop;
// muduo 库中多路事件分发器的核心IO复用模块
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel *>;

    Poller(EventLoop *loop);
    virtual ~Poller() = default;

    // 给所有IO复用保留统一接口
    virtual Timestamp poll(int timeoutMs, ChannelList *activateChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    // 判断参数channel是否在当前poller中
    bool hasChannel(Channel *channel) const;

    // EventLoop可以通过该接口获取默认的IO复用具体实现
    static Poller *newDefaultPoller(EventLoop *loop);

protected:
    using ChannelMap = std::unordered_map<int, Channel *>; //<fd,channel>
    ChannelMap channels_;

private:
    EventLoop *ownerLoop_; // 定义Poller所属的事件循环EventLoop
};