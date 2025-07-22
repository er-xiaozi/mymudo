#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>  //信号量

std::atomic_int Thread::numCreated_(0); // 静态成员变量在类外单独初始化

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_)   //join工作线程 detach分离线程
    {
        thread_->detach();  // thread提供了分离线程的方法
    }
}
void Thread::start()
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);   //信号初始
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        func_(); //开启一个新线程，专门执行该线程函数
    }));

    //等待上面创建的新线程的tid值
    sem_wait(&sem);
}
void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}