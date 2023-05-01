//=====================================================================
//
// ProtocolImp.h - 传输协议实现模块
// 
// NOTE:
// for more information, please see the readme file
//
//=====================================================================
#ifndef __PROTOCOL_IMP_H__
#define __PROTOCOL_IMP_H__

#include "ProtocolBasic.h"
#include "SessionManager.h"
#include "NePingRouter.h"

NAMESPACE_BEGIN(QuickNet)
//---------------------------------------------------------------------
// QuickServer
//---------------------------------------------------------------------
class QuickServer
{
public:
	QuickServer();
	virtual ~QuickServer();

	void SetTrace(Trace *trace);

	bool StartService(int port, const char *ip = NULL);
	void Shutdown();

	// 更新状态，持续调用，建议10ms一次
	void Update(bool Force = false);

	// 发送数据
	bool Send(IINT32 hid, int protocol, const void *data, int size, int limit = -1);

	// 断开连接
	bool Close(IINT32 hid, int code);

	// 取得消息 直接方式，成功返回消息长度，返回-1代表没有消息，-2代表长度错误，
	// 如果 event, wparam, lparam 任意为 NULL 则返回消息长度
	// event会是：QUICKNET_EVENT_NEW/LEAVE/DATA
	int Read(int *event, IUINT32 *wparam, IUINT32 *lparam, void *data, int size);

	// 设置超时
	// idle是空闲超时，-1为不设置，0为取消空闲超时，>0为毫秒时间，超过这个时间收不到包的客户端断开
	// connect连接超时，-1为不设置，0为取消连接超时，>0为毫秒时间，超过这个时间没连接成功的客户端断开
	void SetTimeOut(int idle = -1, int connect = -1);

	// 设置周期，周期越短响应越快，但cpu占用越高
	void SetInterval(int interval);

	// 配置
	int Option(IUINT32 hid, int option, int value);
    int GetOption(IUINT32 hid, int option);

	// 字符串配置
	int Option(IUINT32 hid, const char *options);
    

	// 安装传输层协议
	void SetTransmission(Transmission::Factory factory);

	// 取得本地地址
	void LocalAddress(System::SockAddress &local);

	// 取得某 HID的状态：0关闭，1连接1，2连接2，3连接等待，4建立连接，5关闭等待。-1为错误 hid
	int GetState(IUINT32 hid) const;

	// 广播一群人，返回成功个数
	int GroupCast(const IUINT32 *hid, int count, int protocol, const void *data, int size, int limit = -1);

	// 广播所有人，返回成功个数
	int Broadcast(int protocol, const void *data, int size, int limit = -1);

	// 取得统计信息
	void Statistic(ProtocolUdp::Statistic &stat);

	// 全局掩码
	void SetGlobalMask(IUINT32 mask);

	// 取得KCP等待发送的数据
	int GetPending(IUINT32 hid, int what) const;

protected:
	static void PacketOutput(ProtocolPacket *packet, const System::SockAddress &remote, void *user);
	void UpdateInterval();
	void MakeUUID(int port);

protected:
	Trace *trace;
	IUINT32 _current;
	IUINT32 _slap;
	IUINT32 _interval;
	std::string _uuid;
	int _port;
	ProtocolUdp::Statistic _stat;
	SessionManager *_manager;
	ProtocolUdp _network;
};



//---------------------------------------------------------------------
// QuickClient
//---------------------------------------------------------------------
class QuickClient
{
public:
	QuickClient();
	virtual ~QuickClient();

	void SetTrace(Trace *trace);

	bool Connect(const char *ip, int port);
	void Close();

	// 设置超时
	// idle是空闲超时，-1为不设置，0为取消空闲超时，>0为毫秒时间，超过这个时间收不到包的客户端断开
	// connect连接超时，-1为不设置，0为取消连接超时，>0为毫秒时间，超过这个时间没连接成功的客户端断开
	void SetTimeOut(int idle = -1, int connect = -1);

	// 发送数据，
	// 如果 limit > 0，并且该连接的发送缓存还有 >=limit个待发数据包，则放弃本次发送
	bool Send(int protocol, const void *data, int size, int limit = -1);

	// 接收数据，返回数据长度，没有数据则返回-1
	int Recv(int *protocol, void *data, int size);

	// 更新状态，需要持续调用，建议 10ms一次
	void Update();

	// 设置保活，单位毫秒，小于零为默认值
	void Keepalive(int interval = -1);

	// 安装传输层协议
	void SetTransmission(Transmission::Factory factory);

	// 配置
	int Option(int option, int value);

	// 取得配置
    int GetOption(int  option) const;

	// 字符串配置
	int Option(const char *options);

	// 取得状态：0关闭，1连接1，2连接2，3连接等待，4建立连接，5关闭等待。
	int GetState() const;

	IUINT32 GetHid() const;

	// 是否连接：0为没有连接，1为已经连接
	int IsConnected() const;

	// 取得统计信息
	void Statistic(ProtocolUdp::Statistic &stat);

	// 取得 RTT
	int GetRtt() const;

	// 取得待发送数据
	int GetPending(int what) const;

	/*
	* @packets  总报文数
	* @pull		被拉过包的报文数
	* @pullpkts	实际拉包消息数，当前拉包策略下(仅仅参考)
	* @lost		实际丢包数
	* @pulltimeout 拉包后超时包个数（仅仅参考）
	* @skip		连续包忽略拉包次数
	* @totalskippkt 连续包忽略拉包总个数
	*/
	void GetNACKStatInfo(int* packets, int *pull, int* pullpkts, int *lost, int *pulltimeout, int *skip, int *totalskippkt);

	// 改变传输层的目标 ip:port地址，需要连接成功后调用
	bool SwitchTargetAddress(const char *ip, int port);

	// 取得目标地址，如果传入地址相同，返回真，否则返回假
	bool GetTargetAddress(System::SockAddress &address) const;

    NePinger& GetPingRouter();

	// 全局掩码
	void SetGlobalMask(IUINT32 mask);

protected:
	void CheckOutput();

protected:
	bool _started;
	Trace *trace;
	Timeout _ping_timer;
	Timeout _switch_ip;
	IUINT32 _keepalive;
	IUINT32 _current;
	Session *_session;
	System::SockAddress _target;
	ProtocolUdp::Statistic _stat;
	Transmission::Factory _factory;
	ProtocolUdp _network;
	unsigned char _mask;
	IUINT32 _time_max_idle;
	IUINT32 _time_max_connect;
	NePinger _pinger;
	mutable System::CriticalSection _lock;
};



NAMESPACE_END(QuickNet)


#endif


