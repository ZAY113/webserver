#include "buffer.h"

Buffer::Buffer(int initBuffersize) : buffer_(initBuffersize), readPos_(0), writePos_(0) {}

size_t Buffer::ReadableBytes() const {
    return writePos_ - readPos_;
}

size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;
}

// 前面可以用的空间
size_t Buffer::PrependableBytes() const {
    return readPos_;
}

// 获取当前要读的位置指针
const char* Buffer::Peek() const {
    return BeginPtr_() + readPos_;
}

// 将读位置向后移动len长度
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ += len;
}

// 将读位置移动到end指针的位置
void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end);
    Retrieve(end - Peek());
}

// 清零整个缓冲区数组，重置读写位置
void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}

// 将（当前读位置+可读长度）数据转为string类型并返回，清空缓冲区
std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

// 返回下一次待写的位置指针（返回常量，不可改）
const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

// 返回下一次待写的位置指针（指针可改）
char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

// 写位置向后移动len长度
void Buffer::HasWritten(size_t len) {
    writePos_ += len;
}

// 将常量字符串写入到缓冲区buff中 
void Buffer::Append(const char* str, size_t len) {
    assert(str); // 检查char*指针是否不为空，即buff临时数组是否不为空
    EnsureWriteable(len); // 确保len长度可写
    std::copy(str, str + len, BeginWrite()); // 将常量字符串str的数据拷贝到buff可写位置之后
    HasWritten(len); // 更新写位置
}

// 重载Append函数，使其接收string类型输入
void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

// 重载Append函数，使其接收void*类型指针和字符串长度参数
void Buffer::Append(const void* data, size_t len) {
    assert(data); // 检查确保传入的不是空指针
    Append(static_cast<const char*>(data), len);
}

// 重载Append函数，使其接收Buffer类型参数（另一个缓冲区对象）
// 将传入Buffer的可读数据读入当前Buffer
void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

// 确保可写长度空间足够，若空间不足则扩充
void Buffer::EnsureWriteable(size_t len) {
    if (WritableBytes() < len) {
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len);
}

// 从文件描述符中读取数据到当前缓冲区
ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    char buff[65535];       // 临时的数组，保证能够把所有的数据都读出来
    struct iovec iov[2];    // 两块内存（缓冲区buffer_，临时数组buff），每块内存包含变量(iov[i]_base, iov[i]_len)，表示内存起始地址和内存长度
    const size_t writable = WritableBytes();
    /* 分散读，保证数据全部读完 */
    // 为iov分配每块内存的首地址和长度信息
    iov[0].iov_base = BeginPtr_() + writePos_;
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);
    
    // 系统调用，从文件描述符中分散读
    const ssize_t len = readv(fd, iov, 2);
    if (len < 0) {
        // 分散读失败，返回错误号
        *saveErrno = errno;
    }
    else if (static_cast<size_t>(len) <= writable) {
        writePos_ += len; // 读到的数据小于缓冲区可写空间，则向后移动写位置
    }
    else {
        // 当前缓冲区空间不足，临时数组中存有数据
        writePos_ = buffer_.size(); // 将写位置移到当前缓冲区最后
        Append(buff, len - writable); // 使用临时数组对当前缓冲区进行扩容
    }
    return len;
}

// 将当前缓冲区数据写入到指定的文件描述符中
ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if (len < 0) {
        *saveErrno = errno; // 写失败，返回错误号
        return len;
    }
    readPos_ += len; // 读位置后移
    return len;
}

// 获取buff起始位置指针
char* Buffer::BeginPtr_() {
    return &*buffer_.begin();
}

// 获取buff起始位置的常量指针
const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}

// 调整扩充空间以满足能够容纳新的len长度的数据
void Buffer::MakeSpace_(size_t len) {
    // 若后面可写空间与前面已读取的空间加起来仍不够len，则buff扩充空间
    if (WritableBytes() + PrependableBytes() < len) {
        buffer_.resize(writePos_ + len + 1);
    }
    else { // 否则
        size_t readable = ReadableBytes(); // 获取已写但还未读（可读）字节数
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_()); // 将已写未读数据拷贝到buff起始位置
        readPos_ = 0; // 读位置重置为0
        writePos_ = readPos_ + readable; // 写位置修改
        assert(readable == ReadableBytes()); // 重新调用函数获取新的可读数据字节，并与拷贝前比较，确保相等
    }
}
