//=====================================================================
//
// ProtocolBasic.cpp - protocol packet defintion and I/O
//
// NOTE:
// for more information, please see the readme file
//
//=====================================================================
#include "ProtocolBasic.h"
#include "../system/itoolbox.h"
#include "../system/option.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


NAMESPACE_BEGIN(QuickNet)

//---------------------------------------------------------------------
// ProtocolUdp：接收发送第一层协议的数据包
//---------------------------------------------------------------------
ProtocolUdp::ProtocolUdp()
{
	_buffer = new unsigned char[0x10000];
	trace = &Trace::Global;
	StatisticReset();
	compress_method = IPK_METHOD_LZO;
	compress_level = 5;
	gmask = 0;
}

ProtocolUdp::~ProtocolUdp()
{
	delete []_buffer;
	Close();
}

// 绑定端口
bool ProtocolUdp::Open(int port, const char *ip)
{
	Close();
	if (ip == NULL) ip = "0.0.0.0";
	System::SockAddress remote(ip, port);
	StatisticReset();
	return _transport.open(port, remote.get_ip(), false);
}

// 关闭连接
void ProtocolUdp::Close() 
{
	_transport.close();
}

IUINT32 ProtocolUdp::CheckSum1(const void *data, int size)
{
	const unsigned char *ptr = (const unsigned char*)data;
	IUINT32 checksum = 0;
	for (; size > 0; ptr++, size--) 
		checksum += ptr[0];
	return checksum;
}

IUINT32 ProtocolUdp::CheckSum2(const void *data, int size)
{
	const unsigned char *ptr = (const unsigned char*)data;
	IUINT32 c1 = 0, c2 = 0, c3 = 0, c4 = 0;
	for (; size >= 4; ptr += 4, size -= 4) {
		c1 += ptr[0];
		c2 += ptr[1];
		c3 += ptr[2];
		c4 += ptr[3];
	}
	for (; size > 0; ptr++, size--) {
		c1 += ptr[0];
	}
	return (c1 + c2 + c3 + c4);
}

// 求校验和
IUINT32 ProtocolUdp::CheckSum(const void *data, int size)
{
	IUINT32 checksum = CheckSum2(data, size);
	checksum = (checksum >> 16) + (checksum & 0xffff);
	return (~checksum);
}

// 字节异或
void ProtocolUdp::BytesXOR(void *data, int size, unsigned char mask)
{
	unsigned char *ptr = (unsigned char*)data;
	IUINT32 mm = mask | (((IUINT32)mask) << 8) | (((IUINT32)mask) << 16) | (((IUINT32)mask) << 24);
	// 并行一次 16字节
	for (; size >= 16; ptr += 16, size -= 16) {
		*(IUINT32*)(ptr +  0) ^= mm;
		*(IUINT32*)(ptr +  4) ^= mm;
		*(IUINT32*)(ptr +  8) ^= mm;
		*(IUINT32*)(ptr + 12) ^= mm;
	}
	for (; size >= 4; ptr += 4, size -= 4) {
		*(IUINT32*)ptr ^= mm;
	}
	for (; size > 0; ptr++, size--) {
		*ptr ^= mask;
	}
}

// 发送协议包：成功返回 true，阻塞返回 false
// 内部会增加第一层协议头，发送完会删除 packet对象
bool ProtocolUdp::SendPacket(ProtocolPacket *packet, const System::SockAddress &remote, int compress)
{
	if (packet->head_size() < 4) {
		delete packet;
		return false;
	}

	if (trace->available(TRACE_UDP_BASIC | TRACE_UDP_BYTES)) {
		char text[32];
		remote.string(text);
		if (trace->available(TRACE_UDP_BYTES)) {
			trace->out(TRACE_UDP_BYTES, "[UDP] send (cmd=%d protocol=%x size=%d) to %s:",
				packet->cmd, packet->protocol, packet->size(), text);
			trace->binary(TRACE_UDP_BYTES, packet->data(), packet->size());
		}
		else {
			trace->out(TRACE_UDP_BASIC, "[UDP] send (cmd=%d protocol=%x size=%d) to %s",
				packet->cmd, packet->protocol, packet->size(), text);
		}
	}

	// 压入协议
	packet->push_head_uint8(packet->protocol);

	// 压入命令
	packet->push_head_uint8((packet->cmd & 0x1f) | QUICKNET_CMD_CHECK);

	// 计算并保存 checksum
	IUINT32 checksum = CheckSum(packet->data(), packet->size());
	packet->checksum = (IUINT8)(checksum & 0xff);
	packet->push_head_uint8(packet->checksum);

	// 按掩码异或并保存掩码
	BytesXOR(packet->data(), packet->size(), packet->mask ^ gmask ^ 0x5a);
	packet->push_head_uint8(packet->mask);

	// 发送数据包
	int hr = _transport.send(packet->data(), packet->size(), remote);
	delete packet;

	return (hr > 0)? true : false;
}

// 接收协议包：如果是阻塞则返回 NULL
ProtocolPacket* ProtocolUdp::RecvPacket(System::SockAddress &remote)
{
	int hr = _transport.recv(_buffer, 0x10000, remote);
	if (hr <= 0) return NULL;
	if (hr < 4) {
		InvalidPacket(remote);
		return NULL;
	}
	
	// 先把掩码弹出来，异或后面内容
	IUINT8 mask = _buffer[0] ^ gmask ^ 0x5a;
	BytesXOR(_buffer + 1, hr - 1, mask);

	// 计算并验证 checksum
	IUINT8 check = _buffer[1];
	IUINT32 sum = CheckSum(_buffer + 2, hr - 2);

	if ((IUINT8)(sum & 0xff) != check) {
		if (trace->available(TRACE_UDP_ERROR)) {
			char text[32];
			remote.string(text);
			icrypt_xor_8(_buffer + 1, _buffer + 1, hr - 1, mask);
			trace->out(TRACE_UDP_ERROR, "[UDP] recv error for bad checksum from %s:", text);
			trace->binary(TRACE_UDP_ERROR, _buffer, hr);
		}
		InvalidPacket(remote);
		return NULL;
	}

	// 读取并验证 cmd
	IUINT8 cmd = _buffer[2];
	if ((cmd & 0xe0) != QUICKNET_CMD_CHECK) {
		if (trace->available(TRACE_UDP_ERROR)) {
			char text[32];
			remote.string(text);
			icrypt_xor_8(_buffer + 1, _buffer + 1, hr - 1, mask);
			trace->out(TRACE_UDP_ERROR, "[UDP] recv error for bad cmd from %s:", text);
			trace->binary(TRACE_UDP_ERROR, _buffer, hr);
		}
		InvalidPacket(remote);
		return NULL;
	}

	// 创建协议包
	ProtocolPacket *packet = new ProtocolPacket(hr - 4);
	if (packet == NULL) return NULL;

	packet->push_tail(_buffer + 4, hr - 4);

	packet->mask = mask;
	packet->checksum = check;
	packet->cmd = cmd & 0x1f;
	packet->protocol = _buffer[3];

	if (trace->available(TRACE_UDP_BASIC | TRACE_UDP_BYTES)) {
		char text[32];
		remote.string(text);
		if (trace->available(TRACE_UDP_BYTES)) {
			trace->out(TRACE_UDP_BYTES, "[UDP] recv (cmd=%d protocol=%x size=%d) from %s:",
				packet->cmd, packet->protocol, packet->size(), text);
			trace->binary(TRACE_UDP_BYTES, packet->data(), packet->size());
		}
		else {
			trace->out(TRACE_UDP_BASIC, "[UDP] recv (cmd=%d protocol=%x size=%d) from %s",
				packet->cmd, packet->protocol, packet->size(), text);
		}
	}

	return packet;
}

void ProtocolUdp::InvalidPacket(const System::SockAddress &remote)
{
	printf("INVALID PACKET\n");
}

// PacketList清空
void ProtocolUdp::ClearPacketList(PacketList &plist)
{
	while (plist.begin() != plist.end()) {
		PacketList::iterator it = plist.begin();
		ProtocolPacket *packet = *it;
		plist.erase(it);
		delete packet;
	}
}

// PacketVector清空
void ProtocolUdp::ClearPacketVector(PacketVector &packets)
{
	int count = (int)packets.size();
	for (int i = 0; i < count; i++) {
		ProtocolPacket *packet = packets[i];
		packets[i] = NULL;
		delete packet;
	}
	packets.resize(0);
}

// PacketList 发送
void ProtocolUdp::SendPacketList(PacketList &plist, const System::SockAddress &remote)
{
	while (plist.begin() != plist.end()) {
		PacketList::iterator it = plist.begin();
		ProtocolPacket *packet = *it;
		plist.erase(it);
		SendPacket(packet, remote);
	}
}

// 取得本地地址
void ProtocolUdp::LocalAddress(System::SockAddress &local)
{
	_transport.local(local);
}

// 设置日志
void ProtocolUdp::SetTrace(Trace *trace)
{
	if (trace) {
		this->trace = trace;
	}
}

// 取得缓存长度
bool ProtocolUdp::GetSocketBuffer(int *sndbuf, int *rcvbuf)
{
	return (_transport.get_buffer(sndbuf, rcvbuf) == 0)? true : false;
}

// 设置缓存长度
bool ProtocolUdp::SetSocketBuffer(int sndbuf, int rcvbuf)
{
	return (_transport.set_buffer(sndbuf, rcvbuf) == 0)? true : false;
}

// 统计复位
void ProtocolUdp::StatisticReset()
{
	memset(&_stat_1, 0, sizeof(_stat_1));
	memset(&_stat_2, 0, sizeof(_stat_2));
	memset(&_stat, 0, sizeof(_stat));
	_stat_ts = iclock();
}

// 统计
void ProtocolUdp::StatisticUpdate(Statistic &stat)
{
	// 统计网络到 _stat_2
	_transport.stat(_stat_2);

	// 更新数据
	_stat.out_count = _stat_2.out_count;
	_stat.out_size = _stat_2.out_size;
	_stat.out_data = _stat_2.out_data;
	_stat.in_count = _stat_2.in_count;
	_stat.in_size = _stat_2.in_size;
	_stat.in_data = _stat_2.in_data;
	_stat.discard_count = _stat_2.discard_count;
	_stat.discard_size = _stat_2.discard_size;
	_stat.discard_data = _stat_2.discard_data;

	// 如果离上次统计超过一秒钟，则计算每秒流量
	IUINT32 current = iclock();
	if (itimediff(current, _stat_ts) >= 1000) {
		IINT32 delta = itimediff(current, _stat_ts);
		_stat.per_sec_out_count = ((_stat_2.out_count - _stat_1.out_count) * 1000) / delta;
		_stat.per_sec_out_size = ((_stat_2.out_size - _stat_1.out_size) * 1000) / delta;
		_stat.per_sec_out_data = ((_stat_2.out_data - _stat_1.out_data) * 1000) / delta;
		_stat.per_sec_in_count = ((_stat_2.in_count - _stat_1.in_count) * 1000) / delta;
		_stat.per_sec_in_size = ((_stat_2.in_size - _stat_1.in_size) * 1000) / delta;
		_stat.per_sec_in_data = ((_stat_2.in_data - _stat_1.in_data) * 1000) / delta;
		_stat.per_sec_discard_count = ((_stat_2.discard_count - _stat_1.discard_count) * 1000) / delta;
		_stat.per_sec_discard_size = ((_stat_2.discard_size - _stat_1.discard_size) * 1000) / delta;
		_stat.per_sec_discard_data = ((_stat_2.discard_data - _stat_1.discard_data) * 1000) / delta;
		_stat_ts = current;
		_stat_1 = _stat_2;
	}

	// 输出统计
	stat = _stat;
}


// 设置全局掩码
void ProtocolUdp::SetGlobalMask(IUINT32 mask)
{
	gmask = (unsigned char)(mask & 0xff);
}


//---------------------------------------------------------------------
// Logging：日志输出
//---------------------------------------------------------------------
Trace Trace::Global;
Trace Trace::Null;
Trace Trace::ConsoleWhite(NULL, true, CTEXT_WHITE);
Trace Trace::LogFile("RttTrace_", false, CTEXT_WHITE);
Trace Trace::ConsoleMagenta(NULL, true, CTEXT_BOLD_MAGENTA);
Trace Trace::ConsoleGreen(NULL, true, CTEXT_BOLD_GREEN);

Trace::Trace(const char *prefix, bool STDOUT, int color)
 { 
	_mask = 0; 
	_user = NULL; 
	_buffer = new char [8192];
	_output = NULL; 
	_prefix = NULL;
	_fp = NULL;
	_tmtext = NULL;
	_fntext = NULL;
	_stdout = false;
	_color = -1;
	if (prefix || STDOUT) {
		open(prefix, STDOUT);
	}
	if (color >= 0) {
		this->color(color);
	}
}

Trace::~Trace()
{
	close();
	_mask = 0; 
	_user = NULL; 
	_output = NULL; 
	delete []_buffer;
	_buffer = NULL;
}

void Trace::close()
{
	if (_fp) fclose(_fp);
	if (_prefix) delete []_prefix;
	if (_tmtext) delete []_tmtext;
	if (_fntext) delete []_fntext;
	_fp = NULL;
	_prefix = NULL;
	_tmtext = NULL;
	_fntext = NULL;
	_stdout = false;
	setout(NULL, NULL);
}

void Trace::open(const char *prefix, bool STDOUT)
{
	close();
	if (prefix != NULL) {
		//prefix = "";
		int size = strlen(prefix);
		_prefix = new char [size + 1];
		memcpy(_prefix, prefix, size + 1);
	}
	else {
		if (_prefix) delete _prefix;
		_prefix = NULL;
	}
	_tmtext = new char [64];
	_fntext = new char [1024];
	_saved_date.datetime = 0;
	_fntext[0] = 0;
	_stdout = STDOUT;
	setout(StaticOut, this);
}

void Trace::setout(TraceOut out, void *user)
{
	_output = out;
	_user = user;
}

void Trace::out(int mask, const char *fmt, ...)
{
	if ((mask & _mask) != 0 && _output != NULL) {
		System::CriticalScope scope_lock(_lock);
		va_list argptr;
		va_start(argptr, fmt);
		vsprintf(_buffer, fmt, argptr);
		va_end(argptr);
		_output(_buffer, _user);
	}
}

void Trace::binary(int mask, const void *bin, int size)
{
	static const char hex[17] = "0123456789ABCDEF";
	if ((mask & _mask) != 0 || _output != NULL) {
		System::CriticalScope scope_lock(_lock);
		const unsigned char *src = (const unsigned char*)bin;
		char line[100];
		int count = (size + 15) / 16;
		int offset = 0;
		for (int i = 0; i < count; i++, src += 16, offset += 16) {
			int length = size > 16? 16 : size;
			memset(line, ' ', 99);
			line[99] = 0;
			line[0] = hex[(offset >> 12) & 15];
			line[1] = hex[(offset >>  8) & 15];
			line[2] = hex[(offset >>  4) & 15];
			line[3] = hex[(offset >>  0) & 15];
			for (int j = 0; j < 16 && j < length; j++) {
				int start = 6 + j * 3;
				line[start + 0] = hex[src[j] >> 4];
				line[start + 1] = hex[src[j] & 15];
				if (j == 8) line[start - 1] = '-';
			}
			line[6 + length * 3] = '\0';
			_output(line, _user);
		}
	}
}

void Trace::StaticOut(const char *text, void *user)
{
	Trace *self = (Trace*)user;

	System::DateTime now;

	now.localtime();

	if (now.datetime != self->_saved_date.datetime) {
		self->_saved_date.datetime = now.datetime;
		sprintf(self->_tmtext, "%02d:%02d:%02d:%03d", now.hour(), 
			now.minute(), now.second(), now.millisec());
		int nowday = now.mday() + now.month() * 32;
		if (self->_saved_day != nowday) {
			self->_saved_day = nowday;
			self->_fntext[0] = 0;
		}
	}

	if (self->_prefix) {
		if (self->_fntext[0] == 0) {
			if (self->_fp) fclose(self->_fp);
			self->_fp = NULL;
			sprintf(self->_fntext, "%s%04d%02d%02d.log", 
				self->_prefix, now.year(), now.month(), now.mday());
			self->_fp = fopen(self->_fntext, "a");
			if (self->_fp) {
				fseek(self->_fp, 0, SEEK_END);
			}
		}

		if (self->_fp) {
			fprintf(self->_fp, "[%s] %s\n", self->_tmtext, text);
            fflush(self->_fp);
		}
	}

	if (self->_stdout) {
		if (self->_color >= 0) {
			console_set_color(self->_color);
		}
		printf("[%s] %s\n", self->_tmtext, text);
		if (self->_color >= 0) {
			console_reset();
		}
		fflush(stdout);
	}
}


// 设置颜色，只用于控制台输出(open时 STDOUT=true)，高四位为背景色，低四位为前景色
// 色彩编码见：http://en.wikipedia.org/wiki/ANSI_escape_code
int Trace::color(int color)
{
	int old = _color;
	_color = color;
	return old;
}


NAMESPACE_END(QuickNet)


