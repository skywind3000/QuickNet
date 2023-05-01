//=====================================================================
//
// SessionManager.h - 会话管理模块
// 
// NOTE:
// 使用 SessionDict管理服务端的所有连接，并且提供协议解析还有消息
//
//=====================================================================
#ifndef __SESSION_MANAGER_H__
#define __SESSION_MANAGER_H__

#include "SessionDesc.h"

#include <vector>
#include <string>
#include <map>


NAMESPACE_BEGIN(QuickNet)
//---------------------------------------------------------------------
// 服务端消息
//---------------------------------------------------------------------
#define QUICKNET_EVENT_NEW		0	// 有人加入：wparam = hid
#define QUICKNET_EVENT_LEAVE	1	// 有人离开：wparam = hid
#define QUICKNET_EVENT_DATA		2	// 收到数据：wparam = hid, lparam = protocol

#define QUICKNET_MAX_SESSION	0x3fff	// 最多连接数 16383个连接（不能更改和id生成规则有关）
#define QUICKNET_MAX_BACKLOG	1024	// 最多同时连接数




//---------------------------------------------------------------------
// 会话服务端：根据消息管理 Session和 SessionDict
//---------------------------------------------------------------------
class SessionManager
{
public:
	SessionManager(void *user);
	virtual ~SessionManager();

	void SetTrace(Trace *trace);

	void Update(IUINT32 current, bool force = false, bool flush = false);

	// 输入第一层协议的协议包，解码层第二层协议，传递给其他
	void PacketInput(ProtocolPacket *packet, const System::SockAddress &remote);

	// 取得消息
	ProtocolPacket *GetEvent(bool peek = false);

	// 取得消息 直接方式，成功返回消息长度，返回-1代表没有消息，-2代表长度错误，
	// 如果 event, wparam, lparam 任意为 NULL 则返回消息长度
	// event会是：QUICKNET_EVENT_NEW/LEAVE/DATA
	int ReadEvent(int *event, IUINT32 *wparam, IUINT32 *lparam, void *data, int size);

	// 关闭连接
	bool SessionClose(IUINT32 hid, IUINT32 code);

	// 发送数据
	bool SessionSend(IUINT32 hid, ProtocolPacket *packet, int limit = -1);

	// 发送数据
	bool SessionSend(IUINT32 hid, int protocol, const void *data, int size, int limit = -1);

	// 关闭服务
	void Shutdown();

	// 设置超时
	// idle是空闲超时，-1为不设置，0为取消空闲超时，>0为毫秒时间，超过这个时间收不到包的客户端断开
	// connect连接超时，-1为不设置，0为取消连接超时，>0为毫秒时间，超过这个时间没连接成功的客户端断开
	void SetTimeOut(int idle = -1, int connect = -1);

	// 协议配置
	int Option(IUINT32 hid, int option, int value);
    int GetOption(IUINT32 hid, int option);

	// 设置 Interval
	void SetInterval(int interval);

	// 设置新协议工厂
	void SetTransmission(Transmission::Factory factory);

	// 取得状态：0关闭，1连接1，2连接2，3连接等待，4建立连接，5关闭等待。-1为错误 hid
	int GetState(IUINT32 hid) const;

	// 广播 传入 HID列表，进行广播，返回成功的个数
	int GroupCast(const IUINT32 *hid, int count, int protocol, const void *buffer, int size, int limit = -1);

	// 广播所有人，返回成功的个数
	int Broadcast(int protocol, const void *buffer, int size, int limit = -1);

	// 取得待发送数据
	int GetPending(IUINT32 hid, int what) const;

public:
	void (*PacketOutput)(ProtocolPacket *packet, const System::SockAddress &remote, void *user);

protected:
	void Output(ProtocolPacket *packet, const System::SockAddress &remote);
	void GetIdent(const System::SockAddress &remote, IUINT32 conv, std::string &ident);
	void HandleSyn1(ProtocolPacket *packet, const System::SockAddress &remote);
	void HandleSyn2(ProtocolPacket *packet, const System::SockAddress &remote);

	int SessionForward(Session *session, long limit = -1);	// 检测 Session是否有网络包需要发送出去，有就转发
	void FetchEvents(Session *session);			// 检测 Session是否有应用层数据，有就加入到 _events
	void PostEvent(ProtocolPacket *packet);		// 向 _events链表发送一个事件
	void PostEvent(int event, IUINT32 wparam, IUINT32 lparam, const void *data, int size);

	void UpdateInterval(bool flush);

	void OnConnected(Session *session);
	void OnDisconnected(Session *session, IUINT32 code);
	void OnData(Session *session);

protected:
	IUINT32 _current;
	IUINT32 _slap;
	IUINT32 _interval;
	IUINT32 _time_max_connect;
	IUINT32 _time_max_idle;
	bool _updated;
	void *_user;
	int _nlisten;
	Trace *trace;
	std::vector<IUINT32> _remove;
	typedef std::map<std::string, IUINT32> ListenBacklog;
	Transmission::Factory _factory;
	PacketList _events;
	ListenBacklog _listens;		// ident to hid
	SessionDict _sessions;
};

NAMESPACE_END(QuickNet)


#endif


