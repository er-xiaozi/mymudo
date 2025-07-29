# mymuduo

#### Channel
打包fd和感兴趣的事件(打包loop 和 fd)

#### 事件多路分发器EPollPoller  继承自Poller
负责执行epoll相关操作
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



#### 事件循环（反应堆）EventLoop 
- 重要成员变量
```cpp
std::unique_ptr<Poller> poller_;    //绑定的poller

using ChannelList = std::vector<Channel *>; 
ChannelList activeChannels_;    // eventloop管理的所有channel
std::vector<Functor> pendingFunctors_;    // 存储loop需要执行的所有回调操作

// 主要作用，当mainloop获取一个新用户的channel,通过该成员唤醒subloop处理channel
// 使用了Linux的eventfd()线程间通信，唤醒subloop
int wakeupFd_; 
std::unique_ptr<Channel> wakeupChannel_; // 打包的wakeupfd
```

- 重要接口
```cpp
// 在当前loop中执行
void runInLoop(Functor cb);
// 把cb放入队列中，唤醒loop所在的线程，执行cb
void queueInLoop(Functor cb);

// 唤醒loop所在的线程
void wakeup();

// EventLoop的方法=》 Plooer的方法
void updateChannel(Channel *channel);
void removeChannel(Channel *channel);
bool hasChannel(Channel *channel);

// 判断EventLoop对象是否在自己的线程里面
bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

void doPendingFunctors(); // 执行回调
```
EventLoop::wakeup() 
Reactor 线程正在 epoll_wait 中阻塞，等待 socket 事件。
​另一个线程（非 Reactor 线程）需要向 Reactor 注册一个新的 socket 事件​（比如新连接到来，需要添加到 epoll 监听）。​但 epoll_ctl（添加/修改/删除 fd）必须在 Reactor 线程中执行，因为 epollfd_ 是 Reactor 线程管理的。
wakeup() 的作用就是强制 Reactor 线程从 epoll_wait 中唤醒，让它有机会处理新的事件注册请求。


#### 事件循环EvenLoop创建过程
- 调用EventLoopThread::stratloop()
  该方法创建了一个新线程，新线程中运行threadFunc()方法
- ThreadFunc()执行下面操作:
  - 创建EventLoop loop
  - 执行ThreadInitCallback &cb
  - 通知EventloopThread::startloop()  loop创建完成
  - 开启事件循环loop.loop()
- EventloopThread::startloop() 收到通知后返回新loop指针

#### EventLoopThreadPoll 事件循环线程池
- 重要的成员变量
```cpp
EventLoop *baseLoop_;
std::vector<std::unique_ptr<EventLoopThread>> threads_;
std::vector<EventLoop *> loops_;
```
- 重要接口
```cpp

void setThreadNum(int numThreads) { numThreads_ = numThreads; }

void start(const ThreadInitCallback &cb = ThreadInitCallback());
/*
1、如果设置了线程数量
    创建对应数量的EventLoopThread *t 添加到threads_
    loops_.push_back(t->startLoop) 底层创建线程，绑定一个新的EvenLoop, 并返回loop地址
2、如果没设置线程数量
    直接在baseloop中执行ThreadInitCallback &cb
    cb(baseLoop_)
*/

// 多线程环境中， baseLoop_ 默认以轮询的方式分配channel给subloop
EventLoop *getNextLoop();
```
#### Acceptor
mainReactor 处理新用户的连接，将连接的fd以及感兴趣的events打包成channel发送给subloop
- 重要成员变量
```cpp
EventLoop *loop_;   // baseLoop mainLoop
Socket acceptSocket_;   
Channel acceptChannel_;
bool listenning_;
NewConnectionCallback newConnectionCallback_; // 写在tcpserver中，轮询唤醒subloop，分发连接
```
- 重要成员函数
```cpp
static int createNonblocking()  // 创建listenfd
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
```

##### Socket
socket相关系统调用的封装
```cpp
int fd() const { return sockfd_; }
void bindAddress(const InetAddress &localaddr);
void listen();
int accept(InetAddress *peeraddr);

void shutdownWrite();

void setTcpNoDelay(bool on);
void setReuseAddr(bool on);
void setReusePort(bool on);
void setKeepAlive(bool on);
```

#### TcpServer
相关成员变量
```cpp
using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
EventLoop *loop_; // baseloop 用户定义的loop

const std::string ipPort_;
const std::string name_;

std::unique_ptr<Acceptor> acceptor_;               // 运行在mainLoop, 监听新连接事件
std::shared_ptr<EventLoopThreadPoll> threadPool_; // one loop per thread

ConnectionCallback connectionCallback_;       // 有新连接时的回调
MessageCallbackk messageCallback_;           // 有读写消息时的回调
WriteCompleteCallback writeCompleteCallback_; // 消息发送完成后的回调

ThreadInitCallback threadInitCallback_; // Loop线程初始化的回调
std::atomic_int started_;

int nextConnId_;
ConnectionMap connetions_; // 保存所有的连接
```
相关接口


可能需要修改的地方
void EPollPoller::update(int operation, Channel *channel) 中的memset 改为bzero