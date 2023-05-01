//=====================================================================
//
// RingBuf.h - 
//
// Last Modified: 2019/06/28 15:20:14
//
//=====================================================================
#ifndef _RINGBUF_H_
#define _RINGBUF_H_

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <stdexcept>


namespace AsyncNet {
//---------------------------------------------------------------------
// 环状缓存
//---------------------------------------------------------------------
class RingBuffer
{
public:
	inline virtual ~RingBuffer();
	inline RingBuffer();
	inline RingBuffer(const RingBuffer &ring);
	inline RingBuffer& operator=(const RingBuffer &ring);

#if __cplusplus >= 201100
	inline RingBuffer(RingBuffer &&ring);
#endif


public:
	inline int write(const void *ptr, int size);    // 写入数据
	inline int read(void *ptr, int size);           // 读取数据 (移动指针)
	inline int peek(void *ptr, int size) const;     // 取得数据 (指针不变)
	inline int skip(int size);                      // 移动读指针
	inline int advance(int size);                   // 移动写指针

	inline int size() const;        // 数据长度
	inline int capacity() const;    // 缓存长度
	inline int space() const;    // 可写长度：max(0, capacity - size - 1)

	inline int flat(void **pointer) const;    // 取得空闲平面指针
	inline void reset();                      // 复位
	inline bool resize(int size);             // 改变大小

	// reader 指针加上偏移的位置
	inline unsigned char& operator[](int offset);
	inline const unsigned char& operator[](int offset) const;

	inline bool has_size(int required) const;     // 是否有足够字节数的数据
	inline bool has_space(int required) const;    // 是否有足够字节数的剩余空间

	inline void assert_size(int require) const;     // 没有足够数据就异常
	inline void assert_space(int require) const;    // 没有足够可写空间就异常

	unsigned char& writer(int offset);    // 写指针的引用
	const unsigned char& writer(int offset) const;

	// 强制写入：如果空间不够就 resize
	int forcewrite(const void *ptr, int size);	

	// MSB push data to writer
	inline void push_uint8(uint8_t c);
	inline void push_uint16(uint16_t c);
	inline void push_uint24(uint32_t c);
	inline void push_uint32(uint32_t c);

	// MSB pop data from reader
	inline uint8_t pop_uint8();
	inline uint16_t pop_uint16();
	inline uint32_t pop_uint24();
	inline uint32_t pop_uint32();

	inline uint8_t peek_uint8() const;
	inline uint16_t peek_uint16() const;
	inline uint32_t peek_uint24() const;
	inline uint32_t peek_uint32() const;

protected:
	unsigned char *_data;
	int _size;      // 缓存的长度，是2的整数次方，可用长度为 _size - 1
	int _mask;      // 长度掩码
	int _reader;    // 读指针
	int _writer;    // 写指针
};



//---------------------------------------------------------------------
// 接口实现
//---------------------------------------------------------------------

// 构造
inline RingBuffer::RingBuffer()
{
	_data = NULL;
	_size = 0;
	_mask = 0;
	_reader = 0;
	_writer = 0;
}

inline RingBuffer::RingBuffer(const RingBuffer &ring)
{
	_data = NULL;
	this->operator=(ring);
}

inline RingBuffer& RingBuffer::operator=(const RingBuffer &ring)
{
	if (_data) delete _data;
	_data = new unsigned char[ring._size];
	_size = ring._size;
	_mask = ring._mask;
	_reader = 0;
	_writer = ring.size();
	ring.peek(_data, _writer);
	return *this;
}

#if __cplusplus >= 201100
inline RingBuffer::RingBuffer(RingBuffer &&ring)
{
	_data = ring._data;
	_mask = ring._mask;
	_size = ring._size;
	_reader = ring._reader;
	_writer = ring._writer;
	ring._data = NULL;
	ring._size = 0;
	ring._mask = 0;
	ring._reader = 0;
	ring._writer = 0;
}

#endif


// 析构
inline RingBuffer::~RingBuffer()
{
	if (_data) delete []_data;
	_data = NULL;
	_size = 0;
	_mask = 0;
	_reader = 0;
	_writer = 0;
}

// 有多少字节的数据
inline int RingBuffer::size() const 
{
	return (_size + _writer - _reader) & _mask;
}

// 总缓存长度
inline int RingBuffer::capacity() const
{
	return _size;
}

// 还可以写多少内容
inline int RingBuffer::space() const
{
	int available = _size - size() - 1;
	return (available < 0)? 0 : available;
}

// 读取数据（不删除）
inline int RingBuffer::peek(void *ptr, int size) const
{
	unsigned char *lptr = (unsigned char*)ptr;
	int canread = this->size();
	int half = _size - _reader;

	if (canread == 0) return 0;
	if (size > canread) size = canread;

	if (half >= size) {
		if (lptr) memcpy(lptr, _data + _reader, size);
	}
	else {
		ptrdiff_t half = _size - _reader;
		if (lptr) {
			memcpy(lptr, _data + _reader, half);
			memcpy(lptr + half, _data, size - half);
		}
	}

	return size;
}

// 跳过数据
inline int RingBuffer::skip(int size)
{
	int canread = this->size();
	size = (size > canread)? canread : size;
	_reader = (_reader + size) & _mask;
	return size;
}

// 移动写指针
inline int RingBuffer::advance(int size)
{
	_writer = (_writer + size) & _mask;
	return _writer;
}

// 读取数据并删除
inline int RingBuffer::read(void *ptr, int size)
{
	int canread = this->size();
	if (canread == 0) return 0;
	if (size > canread) size = canread;
	peek(ptr, size);
	_reader = (_reader + size) & _mask;
	return size;
}

// 写数据
inline int RingBuffer::write(const void *ptr, int size)
{
	const unsigned char *lptr = (const unsigned char*)ptr;
	int canwrite = this->space();
	int half = _size - _writer;
	if (canwrite == 0) {
		return 0;
	}
	if (size > canwrite) size = canwrite;
	if (lptr != NULL) {
		if (half >= size) {
			memcpy(_data + _writer, lptr, size);
		}	else {
			memcpy(_data + _writer, lptr, half);
			memcpy(_data, lptr + half, size - half);
		}
	}
	_writer = (_writer + size) & _mask;
	return size;
}


// 取得指针
inline int RingBuffer::flat(void **pointer) const
{
	size_t size = this->size();
	size_t half = _size - _reader;
	if (size == 0) return 0;
	if (pointer) *pointer = (void*)(_data + _reader);
	return (half <= size)? half : size;
}

// 复位指针
inline void RingBuffer::reset()
{
	_reader = _writer = 0;
}

// 调整大小
inline bool RingBuffer::resize(int size)
{
	if (size == 0) {
		if (_data) delete []_data;
		_data = 0;
		_size = _mask = 0;
		_reader = _writer = 0;
	}
	else {
		int require = size + 1;
		int newsize = 64;
		for (newsize = 64; newsize < require; )
			newsize = newsize * 2;
		unsigned char *data = new unsigned char[newsize];
		if (data == NULL) return false;
		int capacity = newsize - 1;
		int dsize = this->size();
		int csize = (dsize < capacity)? dsize : capacity;
		if (dsize > 0) {
			read(data, csize);
		}
		if (_data) delete []_data;
		_data = data;
		_size = newsize;
		_mask = _size - 1;
		_reader = 0;
		_writer = csize;
	}
	return true;
}

inline unsigned char& RingBuffer::operator[](int offset)
{
	return _data[(_reader + offset) & _mask];
}

inline const unsigned char& RingBuffer::operator[](int offset) const
{
	return _data[(_reader + offset) & _mask];
}

inline unsigned char& RingBuffer::writer(int offset)
{
	return _data[(_writer + offset) & _mask];
}

inline const unsigned char& RingBuffer::writer(int offset) const
{
	return _data[(_writer + offset) & _mask];
}


// 强制写入：如果空间不够就 resize
int RingBuffer::forcewrite(const void *ptr, int size)
{
	int canwrite = this->space();
	if (canwrite < size) {
		resize(this->size() + size);
	}
	canwrite = this->space();
	if (canwrite < size) {
		throw std::out_of_range("RingBuffer resize error");
	}
	return write(ptr, size);
}


// 是否有足够字节数的数据
inline bool RingBuffer::has_size(int required) const
{
	return (size() >= required);
}

// 是否有足够字节数的剩余空间
inline bool RingBuffer::has_space(int required) const
{
	return (space() >= required);
}


// 没有足够数据就异常
inline void RingBuffer::assert_size(int required) const
{
	if (size() < required) {
		throw std::length_error("not enough bytes in RingBuffer");
	}
}

// 没有足够可写空间就异常
inline void RingBuffer::assert_space(int required) const
{
	if (space() < required) {
		throw std::length_error("not enough free space in RingBuffer");
	}
}

// MSB push data to writer
inline void RingBuffer::push_uint8(uint8_t c)
{
	assert_space(1);
	writer(0) = c;
	advance(1);
}

inline void RingBuffer::push_uint16(uint16_t c)
{
	assert_space(2);
	writer(0) = (c >> 8) & 0xff;
	writer(1) = (c >> 0) & 0xff;
	advance(2);
}

inline void RingBuffer::push_uint24(uint32_t c)
{
	assert_space(3);
	writer(0) = (c >> 16) & 0xff;
	writer(1) = (c >> 8) & 0xff;
	writer(2) = (c >> 0) & 0xff;
	advance(3);
}

inline void RingBuffer::push_uint32(uint32_t c)
{
	assert_space(4);
	writer(0) = (c >> 24) & 0xff;
	writer(1) = (c >> 16) & 0xff;
	writer(2) = (c >> 8) & 0xff;
	writer(3) = (c >> 0) & 0xff;
	advance(4);
}


// 读取数据但不移动指针
inline uint8_t RingBuffer::peek_uint8() const
{
	assert_size(1);
	uint8_t c = (*this)[0];
	return c;
}

inline uint16_t RingBuffer::peek_uint16() const
{
	assert_size(2);
	uint16_t c1 = (*this)[0];
	uint16_t c2 = (*this)[1];
	return (c1 << 8) | c2;
}

inline uint32_t RingBuffer::peek_uint24() const
{
	assert_size(3);
	uint32_t c1 = (*this)[0];
	uint32_t c2 = (*this)[1];
	uint32_t c3 = (*this)[2];
	return (c1 << 16) | (c2 << 8) | c3;
}

inline uint32_t RingBuffer::peek_uint32() const
{
	assert_size(4);
	uint32_t c1 = (*this)[0];
	uint32_t c2 = (*this)[1];
	uint32_t c3 = (*this)[2];
	uint32_t c4 = (*this)[3];
	return (c1 << 24) | (c2 << 16) | (c3 << 8) | c4;
}


// 读取数据并移动指针
inline uint8_t RingBuffer::pop_uint8()
{
	uint8_t c = peek_uint8();
	skip(1);
	return c;
}

inline uint16_t RingBuffer::pop_uint16()
{
	uint16_t c = peek_uint16();
	skip(2);
	return c;
}

inline uint32_t RingBuffer::pop_uint24()
{
	uint32_t c = peek_uint24();
	skip(3);
	return c;
}

inline uint32_t RingBuffer::pop_uint32()
{
	uint32_t c = peek_uint32();
	skip(4);
	return c;
}


};

#endif



