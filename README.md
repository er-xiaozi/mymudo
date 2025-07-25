# mymudo

### 
- 

 class EPollPoller  继承自Poller
```cpp
//根据channel的index状态调用epoll相关的系统调用，然后修改channel的index状态
void EPollPoller::updateChannel(Channel *channel) 
void EPollPoller::removeChannel(Channel *channel) 
// 更新channel通道
void EPollPoller::update(int operation, Channel *channel)

// Eventloop用ChannelList管理所有的channel, 如果有些channel注册过了，这些channel会写入到Poller的 ChannelMap <fd, channel*>
// 通过EPoll——Wait监听哪些fd(channel)发生了事件，把发生事件的channel通过activatesChannels告知给eventLoop
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)

// 填写活跃的连接
void EPollPoller::fillAciveChannels(int numEvents, ChannelList *activeChannels)
```
获取当前线程的线程id  
CurrentThread::tid()  获取但当前线程的tid
```cpp
//__thread 是 ​GCC（GNU Compiler Collection）​​ 提供的一个扩展关键字，用于声明线程局部变量（Thread-Local Variable）​。
//让变量在每个线程中拥有独立的副本，不同线程之间的访问互不干扰。
__thread int t_cachedTid = 0;
// 通过linux系统调用获取当前tid值
t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
```
EventLoop::wakeup()
Reactor 线程正在 epoll_wait 中阻塞，等待 socket 事件。
​另一个线程（非 Reactor 线程）需要向 Reactor 注册一个新的 socket 事件​（比如新连接到来，需要添加到 epoll 监听）。
​但 epoll_ctl（添加/修改/删除 fd）必须在 Reactor 线程中执行，因为 epollfd_ 是 Reactor 线程管理的。
wakeup() 的作用就是强制 Reactor 线程从 epoll_wait 中唤醒，让它有机会处理新的事件注册请求。


//
可能需要修改的地方
void EPollPoller::update(int operation, Channel *channel) 中的memset 改为bzero