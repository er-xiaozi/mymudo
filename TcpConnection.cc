#include "noncopyable.h"

#include <memory>

class Channel;
class Eventloop;
class Socket;

class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:

private:
    Eventloop *loop_;   // 这里不是mainloop ,
};