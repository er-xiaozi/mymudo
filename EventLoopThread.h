#pragma once

#include "noncopyable.h"
#include "Thread.h"

#include <functional>
#include <mutex>
#include <condition_variable> //条件变量
#include <string>

class EventLoop;

class EventLoopThread : noncopyable
{
public:
    // 定义线程初始化回调类型（函数对象，接受EventLoop指针）
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop *startLoop();

    void threadFunc();

private:
    EventLoop *loop_;              // 指向事件循环对象的指针
    bool exiting_;                 // 标记线程是否正在退出
    Thread thread_;                // 封装的线程对象
    std::mutex mutex_;             // 互斥锁，用于同步
    std::condition_variable cond_; // 条件变量，用于等待事件循环创建完成
    ThreadInitCallback callback_;  // 线程初始化回调函数
};