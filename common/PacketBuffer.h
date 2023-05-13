//=====================================================================
//
// PacketBuffer.h - Linux sk_buff simulation
//
// Last Modified: 2019/06/13 14:43:22
//
//=====================================================================
#ifndef _PACKET_BUFFER_H_
#define _PACKET_BUFFER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#ifndef __cplusplus
#error This file can only be compiled in C++ mode !!
#endif

#include <algorithm>
#include <stdexcept>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../system/system.h"

namespace System {

//=====================================================================
//
// PacketBuffer:
//
// 为了便于多层协议在数据头部或者尾部添加数据，开辟一块稍微大一点的
// 内存 _buffer，并且初始将 _head 指向 _buffer + overhead 处，如果添加
// 头部数据，则往前移动 _head 指针。添加尾部数据则往后移动 _tail 指针
//
//   +----------+--------------------+------+
//   | OVERHEAD |      PAYLOAD       |      |
//   +----------+--------------------+------+
//   0        head                 tail   endup
//
//     <-(push) + (pop)->    <-(pop) + (push)->
//
//=====================================================================
class PacketBuffer
{
public:
	inline PacketBuffer();
	inline PacketBuffer(int capacity);
	inline PacketBuffer(const PacketBuffer &p);
	inline PacketBuffer& operator=(const PacketBuffer &p);

	#if __cplusplus >= 201103 || (defined(_MSC_VER) && _MSC_VER >= 1900)
	inline PacketBuffer(PacketBuffer &&p);
	#endif

	inline virtual ~PacketBuffer();

#ifdef BYTEBUFFER_KMEM
    CLASS_USE_KMEM
#endif

public:
	inline void resize(int capacity);
	inline void reset(int head = 0, int tail = 0);  // _buffer + overhead -> _head/_tail
	inline void swap(PacketBuffer &p);      // swap buffer
	inline void shift(int offset);        // offset: >0 (right), <0 (left)
	inline char* operator[](int pos);	  // 从 _head处开始索引
	inline const char* operator[](int pos) const;
	inline char* data();                      // 取得 _head
	inline const char* data() const;          // 取得 _head
	inline char* ptr_buffer();                // 返回 _head
	inline char* ptr_head();                  // 返回 _head
	inline char* ptr_tail();                  // 返回 _head
	inline char* ptr_endup();                 // 返回 _head
	inline const char* ptr_buffer() const;    // 返回 _head
	inline const char* ptr_head() const;      // 返回 _head
	inline const char* ptr_tail() const;      // 返回 _head
	inline const char* ptr_endup() const;     // 返回 _head
	inline int capacity() const;              // 取得 _endup - _buffer
	inline int size() const;                  // _tail - _head
	inline int head_size() const;             // _head - _buffer
	inline int tail_size() const;             // _endup - _tail

	inline virtual PacketBuffer *copy() const;	// copy self

	inline void move_head(int step);	// _head += step
	inline void move_tail(int step);	// _tail += step

	inline void require_head(int size);    // head 没有足够空间就 exception
	inline void require_tail(int size);    // tail 没有足够空间就 exception
	inline void require_data(int size);    // 没有足够数据就 exception

	inline void push_head(const void *data, int size);	// 上移 _head并插入数据
	inline void push_tail(const void *data, int size);	// 下移 _tail并插入数据

	inline void pop_head(void *data, int size);		// 下移 _head并弹出数据
	inline void pop_tail(void *data, int size);		// 上移 _tail并弹出数据

	inline void push_head_uint8(uint8_t x);
	inline void push_head_uint16(uint16_t x);
	inline void push_head_uint32(uint32_t x);
	inline void push_head_int8(int8_t x);
	inline void push_head_int16(int16_t x);
	inline void push_head_int32(int32_t x);

	inline void push_tail_uint8(uint8_t x);
	inline void push_tail_uint16(uint16_t x);
	inline void push_tail_uint32(uint32_t x);
	inline void push_tail_int8(int8_t x);
	inline void push_tail_int16(int16_t x);
	inline void push_tail_int32(int32_t x);

	inline uint8_t pop_head_uint8();
	inline uint16_t pop_head_uint16();
	inline uint32_t pop_head_uint32();
	inline int8_t pop_head_int8();
	inline int16_t pop_head_int16();
	inline int32_t pop_head_int32();

	inline uint8_t pop_tail_uint8();
	inline uint16_t pop_tail_uint16();
	inline uint32_t pop_tail_uint32();
	inline int8_t pop_tail_int8();
	inline int16_t pop_tail_int16();
	inline int32_t pop_tail_int32();

protected:
	char *_buffer;		// 缓存指针
	char *_head;		// 头部地址
	char *_tail;		// 尾部地址
	char *_endup;		// 结束指针
};


//---------------------------------------------------------------------
// implementation
//---------------------------------------------------------------------

// 重新分配内存
inline void PacketBuffer::resize(int capacity) {
	char *buffer = NULL;
	if (capacity > 0) {
	#ifdef IKMEM_BYTEBUFFER
		buffer = (char*)ikmem_malloc(capacity);
	#else
		buffer = new char[capacity];
	#endif
		if (buffer == NULL) {
			throw std::runtime_error("PacketBuffer: buffer allocation error");
		}
	}
	char *prev_buffer = _buffer;
	char *prev_head = _head;
	char *prev_tail = _tail;
	char *prev_endup = _endup;
	_buffer = buffer;
	_endup = _buffer + capacity;
	_head = std::min(_endup, _buffer + (int)(prev_head - prev_buffer));
	_tail = std::min(_endup, _buffer + (int)(prev_tail - prev_buffer));
	int remain = (int)std::min(_endup - _buffer, prev_endup - prev_buffer);
	if (remain > 0 && _buffer && prev_buffer) {
		assert(_buffer && prev_buffer);
		memcpy(_buffer, prev_buffer, remain);
	}
	if (prev_buffer) {
	#ifdef IKMEM_BYTEBUFFER
		ikmem_free(prev_buffer);
	#else
		delete []prev_buffer;
	#endif
	}
}

// 复位指针：_head = _tail = _buffer + overhead
inline void PacketBuffer::reset(int head, int tail) {
	if (_buffer + head > _endup) {
		throw std::length_error("PacketBuffer: head exceeds capacity");
	}
	if (_buffer + tail > _endup) {
		throw std::length_error("PacketBuffer: head exceeds capacity");
	}
	_head = _buffer + head;
	_tail = _buffer + std::max(head, tail);
}

// dtor
inline PacketBuffer::~PacketBuffer() {
	resize(0);
}

// ctor
inline PacketBuffer::PacketBuffer() {
	_buffer = _head = _tail = _endup = NULL;
}

// ctor
inline PacketBuffer::PacketBuffer(int capacity) {
	_buffer = _head = _tail = _endup = NULL;
	resize(capacity);
}

// copy ctor
inline PacketBuffer::PacketBuffer(const PacketBuffer &p)
{
	_buffer = _head = _tail = _endup = NULL;
	resize(p.capacity());
	reset(p.head_size());
	int size = (int)(p._tail - p._head);
	if (size > 0) {
		if (_head && p._head) {
			memcpy(_head, p._head, size);
		}
		_tail = _head + size;
	}
}

#if __cplusplus >= 201103 || (defined(_MSC_VER) && _MSC_VER >= 1900)
inline PacketBuffer::PacketBuffer(PacketBuffer &&p)
{
	_buffer = _head = _tail = _endup = NULL;
	swap(p);
}
#endif

// copy assignment
inline PacketBuffer& PacketBuffer::operator=(const PacketBuffer &p)
{
	resize(p.capacity());
	reset(p.head_size());
	int size = (int)(p._tail - p._head);
	if (size > 0) {
		if (_head && p._head) {
			memcpy(_head, p._head, size);
		}
		_tail = _head + size;
	}
	return *this;
}

inline void PacketBuffer::swap(PacketBuffer &p)
{
	std::swap(_buffer, p._buffer);
	std::swap(_endup, p._endup);
	std::swap(_head, p._head);
	std::swap(_tail, p._tail);
}

inline void PacketBuffer::shift(int offset)
{
	if (offset == 0) return;
	int capacity = this->capacity();
	int start = this->head_size();
	int size = this->size();
	int newpos = start + offset;
	if (newpos > capacity) newpos = capacity;
	if (newpos + size > capacity) size = capacity - newpos;
	if (newpos < 0) size += newpos, newpos = 0;
	if (size > 0) {
		memmove(_buffer + newpos, _head, size);
	}
	_head = _buffer + newpos;
	_tail = _head + std::max(0, size);
}

inline char* PacketBuffer::operator[](int pos) {
	int position = (int)(_head - _buffer) + pos;
	if (position < 0 || position + _buffer >= _endup) {
		throw std::range_error("PacketBuffer: index error");
	}
	return _buffer + pos;
}

inline const char* PacketBuffer::operator[](int pos) const {
	int position = (int)(_head - _buffer) + pos;
	if (position < 0 || position + _buffer >= _endup) {
		throw std::range_error("PacketBuffer: index error");
	}
	return _buffer + pos;
}

inline char* PacketBuffer::data() {
	return _head;
}

inline const char* PacketBuffer::data() const {
	return _head;
}

inline int PacketBuffer::size() const {
	return (int)(_tail - _head);
}

inline int PacketBuffer::capacity() const {
	return (int)(_endup - _buffer);
}

inline int PacketBuffer::head_size() const {
	return (int)(_head - _buffer);
}

inline int PacketBuffer:: tail_size() const {
	return (int)(_endup - _tail);
}

inline char* PacketBuffer::ptr_buffer() { return _buffer; }
inline char* PacketBuffer::ptr_head() { return _head; }
inline char* PacketBuffer::ptr_tail() { return _tail; }
inline char* PacketBuffer::ptr_endup() { return _endup; }
inline const char* PacketBuffer::ptr_buffer() const { return _buffer; }
inline const char* PacketBuffer::ptr_head() const { return _head; }
inline const char* PacketBuffer::ptr_tail() const { return _tail; }
inline const char* PacketBuffer::ptr_endup() const { return _endup; }

inline PacketBuffer* PacketBuffer::copy() const
{
	PacketBuffer* buffer = new PacketBuffer;
	*buffer = *this;
	return buffer;
}

inline void PacketBuffer::move_head(int step) {
	char *head = _head + step;
	if (head < _buffer || head >= _endup) {
		throw std::range_error("PacketBuffer: head out of range");
	}
	_head = head;
	if (_head > _tail) _tail = _head;
}

inline void PacketBuffer::move_tail(int step) {
	char *tail = _tail + step;
	if (tail < _buffer || tail >= _endup) {
		throw std::range_error("PacketBuffer: tail out of range");
	}
	_tail = tail;
	if (_tail < _head) _head = _tail;
}

// head 没有足够空间就 exception
inline void PacketBuffer::require_head(int size)
{
	if (_head < _buffer + size) {
		throw std::range_error("PacketBuffer: require more heading space");
	}
}

// tail 没有足够空间就 exception
inline void PacketBuffer::require_tail(int size)
{
	if (_tail + size > _endup) {
		throw std::range_error("PacketBuffer: require more tailing space");
	}
}

// 没有足够数据就 exception
inline void PacketBuffer::require_data(int size)
{
	if (this->size() < size) {
		throw std::length_error("PacketBuffer: require more data");
	}
}

inline void PacketBuffer::push_head(const void *data, int size) {
	require_head(size);
	_head -= size;
	if (data) {
		memcpy(_head, data, size);
	}
}

inline void PacketBuffer::push_tail(const void *data, int size) {
	require_tail(size);
	char *tail = _tail;
	_tail += size;
	if (data) {
		memcpy(tail, data, size);
	}
}

inline void PacketBuffer::pop_head(void *data, int size) {
	require_data(size);
	if (data) {
		memcpy(data, _head, size);
	}
	_head += size;
}

inline void PacketBuffer::pop_tail(void *data, int size) {
	require_data(size);
	_tail -= size;
	if (data) {
		memcpy(data, _tail, size);
	}
}

inline void PacketBuffer::push_head_uint8(uint8_t x) { 
	push_head(&x, 1); 
}

inline void PacketBuffer::push_head_uint16(uint16_t x) {
	char buf[2];
	iencode16u_lsb(buf, x);
	push_head(buf, 2); 
}

inline void PacketBuffer::push_head_uint32(uint32_t x) { 
	char buf[4];
	iencode32u_lsb(buf, x);
	push_head(buf, 4);
}

inline void PacketBuffer::push_head_int8(int8_t x) { 
	push_head_uint8((uint8_t)x); 
}

inline void PacketBuffer::push_head_int16(int16_t x) { 
	push_head_uint16((uint16_t)x); 
}

inline void PacketBuffer::push_head_int32(int32_t x) { 
	push_head_uint32((uint32_t)x); 
}

inline void PacketBuffer::push_tail_uint8(uint8_t x) { 
	push_tail(&x, 1);
}

inline void PacketBuffer::push_tail_uint16(uint16_t x) { 
	char buf[2];
	iencode16u_lsb(buf, x);
	push_tail(buf, 2); 
}

inline void PacketBuffer::push_tail_uint32(uint32_t x) {
	char buf[4];
	iencode32u_lsb(buf, x);
	push_tail(buf, 4); 
}

inline void PacketBuffer::push_tail_int8(int8_t x) { 
	push_tail_uint8((uint8_t)x); 
}

inline void PacketBuffer::push_tail_int16(int16_t x) { 
	push_tail_uint16((uint16_t)x); 
}

inline void PacketBuffer::push_tail_int32(int32_t x) { 
	push_tail_uint32((uint32_t)x); 
}

inline uint8_t PacketBuffer::pop_head_uint8() {
	uint8_t x;
	pop_head(&x, 1);
	return x;
}

inline uint16_t PacketBuffer::pop_head_uint16() {
	uint16_t x;
	char buf[2];
	pop_head(buf, 2);
	idecode16u_lsb(buf, &x);
	return x;
}

inline uint32_t PacketBuffer::pop_head_uint32() {
	uint32_t x;
	char buf[4];
	pop_head(buf, 4);
	idecode32u_lsb(buf, &x);
	return (uint32_t)x;
}

inline int8_t PacketBuffer::pop_head_int8() { 
	return (int8_t)pop_head_uint8(); 
}

inline int16_t PacketBuffer::pop_head_int16() { 
	return (int16_t)pop_head_uint16(); 
}

inline int32_t PacketBuffer::pop_head_int32() { 
	return (int32_t)pop_head_uint32(); 
}

inline uint8_t PacketBuffer::pop_tail_uint8() {
	uint8_t x;
	pop_tail(&x, 1);
	return x;
}

inline uint16_t PacketBuffer::pop_tail_uint16() {
	uint16_t x;
	char buf[2];
	pop_tail(buf, 2);
	idecode16u_lsb(buf, &x);
	return (uint16_t)x;
}

inline uint32_t PacketBuffer::pop_tail_uint32() {
	uint32_t x;
	char buf[4];
	pop_tail(buf, 4);
	idecode32u_lsb(buf, &x);
	return (uint32_t)x;
}

inline int8_t PacketBuffer::pop_tail_int8() {
	return (int8_t)pop_tail_uint8();
}

inline int16_t PacketBuffer::pop_tail_int16() {
	return (int16_t)pop_tail_uint16();
}

inline int32_t PacketBuffer::pop_tail_int32() {
	return (int32_t)pop_tail_uint32();
}


};



#endif



