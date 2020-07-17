#include "Buffer.hpp"

#include "ConnectionContext.hpp"
#include "UDPRelay.hpp"
#include "ssrutils.h"
#include "uvw/stream.h"

#include <algorithm>
namespace
{
void freeBuf(buffer_t* buf)
{
    free(buf->data);
    free(buf);
}
} // namespace
Buffer::Buffer()
    : buf { newBuf(), freeBuf }
{
}

size_t* Buffer::getLengthPtr()
{
    return &buf->len;
}

buffer_t* Buffer::newBuf()
{
    auto bufPtr = reinterpret_cast<buffer_t*>(malloc(sizeof(buffer_t)));
    balloc(bufPtr, Buffer::BUF_DEFAULT_CAPACITY);
    bufPtr->capacity = Buffer::BUF_DEFAULT_CAPACITY;
    bufPtr->len = 0;
    bufPtr->idx = 0;
    return bufPtr;
}

char Buffer::operator[](int idx)
{
    return buf->data[idx % (buf->capacity)];
}

char** Buffer::getBufPtr()
{
    return &buf->data;
}

char* Buffer::back()
{
    if (buf)
        return buf->data + buf->len;
    return nullptr;
}

char* Buffer::begin()
{
    return buf->data;
}

void Buffer::clear()
{
    buf->len = 0;
}

void Buffer::drop(size_t size)
{
    if (buf->len < size)
        return;
    memmove(buf->data, buf->data + size, buf->len - size);
    buf->len -= size;
}

void Buffer::bufRealloc(size_t size)
{
    buf->data = reinterpret_cast<char*>(realloc(buf->data, size * sizeof(char)));
    buf->capacity = size;
    buf->len = buf->capacity < buf->len ? buf->capacity : buf->len;
}

std::unique_ptr<char[]> Buffer::duplicateDataToArray()
{
    std::unique_ptr<char[]> data { new char[buf->len]() };
    memcpy(data.get(), buf->data, buf->len);
    return data;
}

void Buffer::copy(const uvw::DataEvent& event)
{
    if (event.length == 0)
        return;
    if (event.length + buf->len <= buf->capacity) {
        this->copy(event.data.get(), event.data.get() + event.length);
        return;
    } else if (buf->len < buf->capacity) {
        bufRealloc(event.length * 2);
        buf->capacity = event.length * 2;
        this->copy(event.data.get(), event.data.get() + event.length);
        return;
    }
}

void Buffer::copy(const uvw::UDPDataEvent& event)
{
    if (event.length == 0)
        return;
    if (event.length + buf->len <= buf->capacity) {
        this->copy(event.data.get(), event.data.get() + event.length);
        return;
    } else if (buf->len < buf->capacity) {
        bufRealloc(event.length * 2);
        buf->capacity = event.length * 2;
        this->copy(event.data.get(), event.data.get() + event.length);
        return;
    }
}

void Buffer::copyFromBegin(const uvw::DataEvent& event, int length)
{
    auto start = event.data.get();
    auto size = length == -1 ? event.length : length;
    memcpy(begin(), start, size);
    buf->len = size;
}

void Buffer::copyFromBegin(char* start, size_t size)
{
    memcpy(begin(), start, size);
    buf->len = size;
}

void Buffer::copy(const Buffer& that)
{
    memcpy(buf->data, that.buf->data, that.buf->len);
    buf->len = that.buf->len;
}

void Buffer::setLength(int l)
{
    buf->len = l;
}

size_t Buffer::length()
{
    return buf->len;
}

int Buffer::ssEncrypt(CipherEnv& cipherEnv, ConnectionContext& connectionContext)
{
    int err = cipherEnv.crypto->encrypt(buf.get(), connectionContext.e_ctx.get(), BUF_DEFAULT_CAPACITY);
    return err;
}

int Buffer::ssDecrypt(CipherEnv& cipherEnv, ConnectionContext& connectionContext)
{
    int err = cipherEnv.crypto->decrypt(buf.get(), connectionContext.d_ctx.get(), BUF_DEFAULT_CAPACITY);
    return err;
}

size_t* Buffer::getCapacityPtr()
{
    return &buf->capacity;
}

void Buffer::copy(char* start, char* end)
{
    memcpy(back(), start, end - start);
    buf->len += end - start;
}

int Buffer::ssEncryptAll(CipherEnv& cipherEnv)
{
    int err = cipherEnv.crypto->encrypt_all(buf.get(), cipherEnv.crypto->cipher, UDPRelay::DEFAULT_PACKET_SIZE * 2);
    return err;
}

int Buffer::ssDecryptALl(CipherEnv& cipherEnv)
{
    int err = cipherEnv.crypto->decrypt_all(buf.get(), cipherEnv.crypto->cipher, UDPRelay::DEFAULT_PACKET_SIZE * 2);
    return err;
}

char* Buffer::end()
{
    if (buf)
        return buf->data + buf->capacity;
    return nullptr;
}
