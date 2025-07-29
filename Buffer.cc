#include "Buffer.h"

#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>

// 从fd上读取数据，  Poller工作在LT模式
// Buffer缓冲区是有大小的！但是在fd上读数据的时候不知道数据最终大小
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    char extrabuf[65536] = {0}; // 栈上的内存空间
    // readv() 可以根据读出的数据自动填充多个缓冲区
    struct iovec vec[2];
    const size_t writable = writableBytes(); // 这是Buffer底层缓冲区剩余可写空间大小
    vec[0].iov_base = begin() + writeIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writable) //buf可写缓冲区足够存储数据 
    {
        writeIndex_ += n;
    }
    else // extrabuf 也写入的数据
    {
        writeIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if(n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}