//=====================================================================
//
// ProtocolBasic.h - protocol packet defintion and I/O
//
// NOTE:
// for more information, please see the readme file
//
//=====================================================================
#ifndef __PROTOCOL_BASIC_H__
#define __PROTOCOL_BASIC_H__

#include "PacketBuffer.h"
#include "TransportUdp.h"
#include "../system/itoolbox.h"

#include <vector>
#include <list>

NAMESPACE_BEGIN(QuickNet)
#define QUICKNET_OVERHEAD	48

//---------------------------------------------------------------------
// ProtocolPacket：协议数据包
//---------------------------------------------------------------------
class ProtocolPacket : public PacketBuffer
{
public:
	ProtocolPacket(int size): PacketBuffer(size, QUICKNET_OVERHEAD) {}

	// 第一层协议：验证数据包合法性，区别命令
	IUINT8 mask;		// 掩码：将会和包内所有字节做异或
	IUINT8 checksum;	// 验证：将所有二层协议以内的东西加起来
	IUINT8 cmd;			// 命令编号
	IUINT8 protocol;	// 协议编号
	// 第二层协议：判断命令：建立连接的关键部分
	IUINT32 conv;		// 会话编号
	IUINT32 hid;		// 目标
	// 第三层协议：FEC 
	IUINT32 fec_v1;
	IUINT32 fec_v2;
	IUINT32 fec_v3;
	IUINT32 fec_v4;
	// 第四层协议：拉包
	IUINT32 sn;
	IUINT8 flag; 
	// 反馈应用层协议
	int event;
	IUINT32 wparam;
	IUINT32 lparam;

	ProtocolPacket* copy() const {
		ProtocolPacket *p = new ProtocolPacket(_endup - _head);
		if (_tail > _head) p->push_tail(_head, _tail - _head);
		p->mask = mask;
		p->checksum = checksum;
		p->cmd = cmd;
		p->protocol = protocol;
		p->conv = conv;
		p->hid = hid;
		p->fec_v1 = fec_v1;
		p->fec_v2 = fec_v2;
		p->fec_v3 = fec_v3;
		p->fec_v4 = fec_v4;
		p->sn = sn;
		p->flag = flag;
		p->event = event;
		p->wparam = wparam;
		p->lparam = lparam;
		return p;
	}
};

#define QUICKNET_FLAG_DATA		1		// 标志：数据
#define QUICKNET_FLAG_PULL		2		// 标志：拉包

#define QUICKNET_CMD_SYN1		0x16	// 命令：连接请求
#define QUICKNET_CMD_ACK1		0x12	// 命令：连接反馈
#define QUICKNET_CMD_SYN2		0x19	// 命令：连接确认
#define QUICKNET_CMD_ACK2		0x17	// 命令：连接建立
#define QUICKNET_CMD_PING		0x18	// 命令：PING发送
#define QUICKNET_CMD_PACK		0x14	// 命令：PING返回
#define QUICKNET_CMD_DATA		0x11	// 命令：协议数据
#define QUICKNET_CMD_FIN		0x15	// 命令：结束
#define QUICKNET_CMD_FACK		0x13	// 命令：结束验证
#define QUICKNET_CMD_CHGIP		0x1A	// 命令：改变IP
#define QUICKNET_CMD_CHACK		0x1B	// 命令：改变IP返回
#define QUICKNET_CMD_HELLO		0x1D	// 命令：HELLO
#define QUICKNET_CMD_HBACK		0x1C	// 命令：HELLO BACK
#define QUICKNET_CMD_CHECK		0xA0	// 命令头部验证

#define QUICKNET_PROTOCOL_RAW		0		// 协议：原始 UDP
#define QUICKNET_PROTOCOL_KCP		1		// 协议：KCP协议
#define QUICKNET_PROTOCOL_TCP		2		// 协议：TCP协议
#define QUICKNET_PROTOCOL_NACK		3		// 协议：NACK协议
#define QUICKNET_PROTOCOL_FEC		0xff	// 协议：FEC协议
#define QUICKNET_PROTOCOL_COMBINE	0xee	// 协议：多帧协议

#define SIZE_IP_OVERHEAD			20
#define SIZE_UDP_OVERHEAD			8

#define QUICKNET_SESSION_OVERHEAD	12
#define QUICKNET_FEC_OVERHEAD		16


//---------------------------------------------------------------------
// 包列表
//---------------------------------------------------------------------
typedef std::list<ProtocolPacket*> PacketList;
typedef std::vector<ProtocolPacket*> PacketVector;
class Trace;


//---------------------------------------------------------------------
// ProtocolUdp：接收发送第一层协议的数据包
//---------------------------------------------------------------------
class ProtocolUdp
{
public:
	ProtocolUdp();
	virtual ~ProtocolUdp();
	
	// 绑定端口
	bool Open(int port, const char *ip = NULL);

	// 关闭连接
	void Close();

	// 发送协议包：成功返回 true，阻塞返回 false
	// 内部会增加第一层协议头，发送完会删除 packet对象
	bool SendPacket(ProtocolPacket *packet, const System::SockAddress &remote, int compress = 0);

	// 接收协议包：如果是阻塞则返回 NULL
	ProtocolPacket* RecvPacket(System::SockAddress &remote);

	// PacketList清空
	static void ClearPacketList(PacketList &plist);

	// PacketVector 清空
	static void ClearPacketVector(PacketVector &packets);

	// PacketList 发送
	void SendPacketList(PacketList &plist, const System::SockAddress &remote);

	// 取得本地地址
	void LocalAddress(System::SockAddress &local);

	// 设置日志
	void SetTrace(Trace *trace);

	// 取得缓存长度
	bool GetSocketBuffer(int *sndbuf = NULL, int *rcvbuf = NULL);

	// 设置缓存长度
	bool SetSocketBuffer(int sndbuf = -1, int rcvbuf = -1);

	// 设置全局掩码
	void SetGlobalMask(IUINT32 mask);

public:
	struct Statistic		// 收发包统计
	{
		IINT64 out_count;
		IINT64 out_size;
		IINT64 out_data;
		IINT64 in_count;
		IINT64 in_size;
		IINT64 in_data;
		IINT64 discard_count;
		IINT64 discard_size;
		IINT64 discard_data;
		IINT64 per_sec_out_count;
		IINT64 per_sec_out_size;
		IINT64 per_sec_out_data;
		IINT64 per_sec_in_count;
		IINT64 per_sec_in_size;
		IINT64 per_sec_in_data;
		IINT64 per_sec_discard_count;
		IINT64 per_sec_discard_size;
		IINT64 per_sec_discard_data;
		IINT64 compress_src;
		IINT64 compress_out;
	};

	// 统计
	void StatisticUpdate(Statistic &stat);

	// 统计复位
	void StatisticReset();

protected:
	static IUINT32 CheckSum(const void *data, int size);
	static IUINT32 CheckSum1(const void *data, int size);
	static IUINT32 CheckSum2(const void *data, int size);
	static void BytesXOR(void *data, int size, unsigned char mask);
	void InvalidPacket(const System::SockAddress &remote);

protected:
	TransportUdp _transport;
	TransportUdp::statistic _stat_1;
	TransportUdp::statistic _stat_2;
	Statistic _stat;
	IUINT32 _stat_ts;
	Trace *trace;
	int compress_method;
	int compress_level;
	std::string compressed;
	unsigned char gmask;
	unsigned char *_buffer;
};


#define QUICKNET_OPT_FEC_MAXBUFSIZE   0x1100
#define QUICKNET_OPT_FEC_BUFITEM_NUM  0x1101 
#define QUICKNET_OPT_FEC_MAXK         0x1102  
#define QUICKNET_OPT_FEC_ENABLED      0x1103  //0 : fasle : non 0; true;
#define QUICKNET_OPT_FEC_SORTED       0x1104  //0 : fasle : non 0; true;
#define QUICKNET_OPT_FEC_LOST_RATE    0x1107  //[0-100], based on 100;
#define QUICKNET_OPT_FEC_STATIC_K            0x1108  
#define QUICKNET_OPT_FEC_STATIC_N            0x1109  
#define QUICKNET_OPT_FEC_DYNKN        0x110A 
#define QUICKNET_OPT_FEC_RECV_PKT     0x110B 
#define QUICKNET_OPT_FEC_FEC_RESTORE_PKT  0x110C 


//---------------------------------------------------------------------
// Transmission：链路层协议（由 Session调用）
//---------------------------------------------------------------------
class Transmission
{
public:
	Transmission() { user = NULL; PacketOutput = NULL; PacketDeliver = NULL;}
	virtual ~Transmission() {};

    // 协议工厂
	typedef Transmission* (*Factory)();

	// 输出下层协议的 packet，函数指针，外面提供
	void (*PacketOutput)(ProtocolPacket *packet, void *user);

	// 输出上层协议的 packet，函数指针，外面提供
	void (*PacketDeliver)(ProtocolPacket *packet, void *user);

	// 用户指针，用来调用 PacketOutput时传入最后的参数
	void *user;
    
	// 输出下层数据包
	inline void Output(ProtocolPacket *packet) { 
		if (PacketOutput) PacketOutput(packet, user);
		else delete packet;
	}

	// 输出上层数据包
	inline void Deliver(ProtocolPacket *packet) {
		if (PacketDeliver) PacketDeliver(packet, user);
		else delete packet;
	}

	// 输入下层协议的 packet，由外层调用
	virtual void PacketInput(ProtocolPacket *packet) = 0;

	// 发送上层数据
	virtual void Send(ProtocolPacket *packet) = 0;

	// 更新状态
	virtual void Update(IUINT32 current) = 0;

	// 返回 OVERHEAD
	virtual int GetOverhead() const = 0;

	// 设置
	virtual int Option(int option, int value) = 0;

	// 取得状态
	virtual int GetStatus(int option) const = 0;
};


//---------------------------------------------------------------------
// 日志输出
//---------------------------------------------------------------------
class Trace
{
public:
	Trace(const char *prefix = NULL, bool STDOUT = false, int color = -1);
	virtual ~Trace();

	typedef void (*TraceOut)(const char *text, void *user);

	bool available(int mask) const { return ((_mask & mask) && _output); }

	void setmask(int mask) { _mask = mask; }
	void enable(int mask) { _mask |= mask; }
	void disable(int mask) { _mask &= ~mask; }

	void setout(TraceOut out, void *user);
	void out(int mask, const char *fmt, ...);
	void binary(int mask, const void *bin, int size);

	// 如果 prefix == NULL则不向文件输出
	void open(const char *prefix, bool STDOUT = false);
	void close();

	// 设置颜色，只用于控制台输出(open时 STDOUT=true)，高四位为背景色，低四位为前景色
	// 色彩编码见：http://en.wikipedia.org/wiki/ANSI_escape_code，返回先前颜色
	int color(int color = -1);

	static Trace Global;
	static Trace Null;
	static Trace ConsoleWhite;
    static Trace LogFile;
	static Trace ConsoleMagenta;
	static Trace ConsoleGreen;

protected:
	static void StaticOut(const char *text, void *user);

protected:
	TraceOut _output;
	System::DateTime _saved_date;
	void *_user;
	char *_buffer;
	char *_prefix;
	bool _stdout;
	int _saved_day;
	FILE *_fp;
	char *_tmtext;
	char *_fntext;
	int _color;
	System::CriticalSection _lock;
	int _mask;
};


#define TRACE_ERROR				1
#define TRACE_WARNING			2
#define TRACE_MGR_PACKET		4
#define TRACE_MGR_SYN			8
#define TRACE_MGR_EVENT			16
#define TRACE_SESSION			32
#define TRACE_KCP				64
#define TRACE_SERVER			128
#define TRACE_CLIENT			256
#define TRACE_UDP_BASIC			512
#define TRACE_UDP_BYTES			1024
#define TRACE_UDP_ERROR			2048
#define TRACE_RTT_REPORT        8192


NAMESPACE_END(QuickNet)


#endif


