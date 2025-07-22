#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include <memory>

EventLoopThreadPoll::EventLoopThreadPoll(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0)
{}
EventLoopThreadPoll::~EventLoopThreadPoll()
{
}

void EventLoopThreadPoll::start(const ThreadInitCallback &cb)
{
    started_ = true;

    for (int i = 0; i<numThreads_; ++i)
    {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        EventLoopThread* t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop());   // 底层创建线程，绑定一个新的eventloop, 并返回该loop地址
    }
    // 只有一个线程，没有setThread num
    if (numThreads_ == 0 && cb)
    {
        cb(baseLoop_);
    }
}

// 多线程环境中， baseLoop_ 默认以轮询的方式分配channel给subloop
EventLoop *EventLoopThreadPoll::getNextLoop()
{
    EventLoop *loop = baseLoop_;

    if (!loops_.empty())    //如果设置了线程数量
    {
        loop = loops_[next_];
        ++next_;
        if (next_ >= loops_.size())
        {
            next_ = 0;
        }
    }

    return loop;
}

std::vector<EventLoop *> EventLoopThreadPoll::getAllLoops()
{
    if (loops_.empty())
    {
        return std::vector<EventLoop*>(1, baseLoop_);
    }
    else
    {
        return loops_;
    }
}