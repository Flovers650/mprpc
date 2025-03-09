#include "Buffer.hpp"
#include <sys/uio.h>
#include <unistd.h>


ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    char extrabuf[65536] = {0}; //栈上的内存空间，64k
    iovec vec[2];

    const size_t writable = writableBytes();
    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const ssize_t n = readv(fd, vec, iovcnt);

    if(n < 0)
    {
        *saveErrno = errno;
    }
    else if(n <= writable)
    {
        writerIndex_ += n;
    }
    else //extrabuf 里面也写入了数据
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable); //writeIndex_开始写 n - writable 大小的数据
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    ssize_t n = write(fd, peek(), readableBytes());
    if(n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}