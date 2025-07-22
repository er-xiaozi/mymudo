#pragma once

#include "noncopyable.h"

#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>

class Thread
{
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc, const std::string &name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const { return started_; }
    pid_t tid() const { return tid_; }
    const std::string &name() { return name_; }

    static int numCreated() { return numCreated_; }

private:
    void setDefaultName();

    bool started_;
    bool joined_;
    // std::thread thread_; 线程会直接启动，不能控制启动时机，所以需要智能指针
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    ThreadFunc func_; // 线程函数
    std::string name_;
    static std::atomic_int numCreated_;
};