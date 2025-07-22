#pragma once

#include <cstddef> 
#include <vector>
#include <string>
#include <algorithm>

// 网络层底层缓冲区
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writeIndex_(kCheapPrepend)
    {}

    size_t readableBytes() const
    {
        return writeIndex_ - readerIndex_;
    }

    size_t writableBytes() const
    {
        return buffer_.size() - readerIndex_;
    }

    size_t prependableBytes() const
    {
        return readerIndex_;
    }
    // 缓冲区中可读地址的起始地址
    const char* peek() const 
    {
        return begin() + readerIndex_;
    }

    // string <- Buffer
    void retrieve(size_t len)
    {
        if(len < readableBytes())   // 应用只读取了len长度的一部分
        {
            readerIndex_ += len; 
        }
        else    //len == readableBytes()
        {
            retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_ = writeIndex_ = kCheapPrepend;
    }

    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());
    }
    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);//读取缓冲区可读数据
        retrieve(len);
        return result;
    }

    void ensureWriteableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len); //扩容函数
        }
    }

    // 把[data, data+len]内存上的数据添加到缓冲区中
    void append(const char *data, size_t len)
    {
        ensureWriteableBytes(len);
        std::copy(data, data + len, beginWrite());
        writeIndex_ += len;
    }

    char* beginWrite()
    {
        return begin() + writeIndex_;
    }

    const char* beginWrite() const
    {
        return begin() + writeIndex_;
    }

    ssize_t readFd(int fd, int* saveErrno);
private:
    char* begin()
    {   
        // it.operator*()
        return &*buffer_.begin(); //vector底层首元素的地址
    }
    const char* begin() const
    {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len)
    {
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writeIndex_ + len);
        }
        else    //把未读数据搬到已读数据的位置
        {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,
                    begin() + writeIndex_,
                    begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writeIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writeIndex_;
};