//=====================================================================
//
// SessionDesc.h - 会话模块，一个 Session就是一个链接
// 
// NOTE:
// Session 类用于描述一条 UDP连接，负责连接握手，连接保活，以及链路层
// 传输，这是一个纯协议，没有网络部分。
// SessionDict 是服务端用于管理所有连接的一个字典，用32位整数 hid来管
// 理每一条连接的 Session对象。
// 当接收到下层数据时，调用 PacketInput，PacketOutput用于输出到下层协
// 议，上层协议发送调用  Send，上层协议接收调用 Recv
//
//=====================================================================
#ifndef __SESSION_DESC_H__
#define __SESSION_DESC_H__

#include "ProtocolBasic.h"
#include "../system/inetkcp.h"
#include "../system/inettcp.h"
#include "RequestRepeat.h"
#include "Combinator.h"

#include <string>
#include <list>


NAMESPACE_BEGIN(QuickNet)
static const IUINT32 SESSION_MAX_CONN_TIME = 700000;		// 默认最长连接时间 10秒
static const IUINT32 SESSION_MAX_IDLE_TIME = 70000;		// 默认最长空闲时间 70秒


// 断开编码
#define QNET_CODE_TIMEOUT	81
#define	QNET_CODE_CONNECT_FAIL 82
#define QNET_CODE_RESEND_FAIL  83

//---------------------------------------------------------------------
// 连接状态
//---------------------------------------------------------------------
#define QUICK_SESSION_STATE_CLOSED		0	// 关闭
#define QUICK_SESSION_STATE_SYN1		1	// 客户端：连接1
#define QUICK_SESSION_STATE_SYN2		2	// 客户端：连接2
#define QUICK_SESSION_STATE_SYNWAIT		3	// 服务端：半连接
#define QUICK_SESSION_STATE_ESTAB		4	// 建立连接
#define QUICK_SESSION_STATE_FINWAIT		5	// 等待关闭


//---------------------------------------------------------------------
// 链接描述符：解析第二层协议
//---------------------------------------------------------------------
class Session
{
public:
	Session(IUINT32 hid, IUINT32 conv);
	virtual ~Session();

	IUINT32 hid() const { return _hid; }
	IUINT32 conv() const { return _conv; }
	int state() const { return _state; }

	//是否正在连接；
	bool IsConnecting() const;
	IINT32 LifeTime(IUINT32 current) const { 
		if (_updated == false) return 0;
		return itimediff(_current, _tscreate);
	}

	IINT32 IdleTime(IUINT32 current) const {
		if (_updated == false) return 0;
		return itimediff(_current, _tsactive);
	}

	void SetTrace(Trace *trace);

	// 更新状态
	void Update(IUINT32 current, bool flush = true);

	// 输入并处理第二层协议（已经解出 hid, conv的数据包）
	void PacketInput(ProtocolPacket *packet);

	// 连接远端地址
	bool Connect(System::SockAddress &remote, IUINT32 conv);

	// 断开连接
	bool Disconnect();

	// 接受连接
	bool AcceptSyn1(const System::SockAddress &remote);

	// 发送消息：需要带上 protocol
	bool Send(ProtocolPacket *packet, int limit = -1);

	// 发送数据：
	bool Send(int protocol, const void *data, int size, int limit = -1);

	// 接收消息
	ProtocolPacket* Recv(bool peek = false);

	// 开始 PING
	void RemotePing();

	// 接收消息 直接接收
	// 成功返回消息长度，没有消息则返回-1，长度错误返回 -2
	// protocol为null则返回消息长度
	int Recv(int *protocol, void *data, int size);

	// 设置 MTU
	void SetMTU(int mtu);

	// 协议更新
	int Option(int option, int value);

    int GetOption(int option) const;

	// 安装协议
	bool TransmissionInstall(Transmission::Factory factory);

	// 取得 RTT
	int GetRtt() const;

	// 取得 KCP的待发送数据
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


protected:
	// 发送协议命令
	void SendCommand(int command, const void *data = NULL, int size = -1);

	// 加入第二层协议头：conv, hid并放入 output队列
	void PacketOutput(ProtocolPacket *packet);

	void OnConnected();

	void OnDisconnected();

	void InputCommandData(ProtocolPacket *packet);

	void InputCommandPack(ProtocolPacket *packet);

	// 自动判断是否有传输层 Transmission，否则直接调用 PacketOutput
	void Transport(ProtocolPacket *packet);

	// 传输层：接收上一层数据
	static void TransmissionDeliver(ProtocolPacket *packet, void *user);

	// 传输层：发送下一层数据
	static void TransmissionOutput(ProtocolPacket *packet, void *user);

	// 合并数据包传输，判断是否开启合并，开启的话就使用合并，下层调用 Transport
	void CombineTransport(ProtocolPacket *packet);

	// 接收到 Combinator 数据
	void CombineInput(ProtocolPacket *packet);

	// 合并层：传递上一层数据
	static void CombinatorDeliver(const void *data, int size, int protocol, void *user);

	// 合并层：发送下一层数据
	static void CombinatorOutput(ProtocolPacket *packet, void *user);

	void ProtocolInit();
	void ProtocolDestroy();
	bool ProtocolSend(int protocol, const void *data, int size, int limit = -1);
	void ProtocolUpdate(bool flush);
	void ProtocolInput(ProtocolPacket *packet);
	void ProtocolFlush();

	static int TcpOutput(const char *buf, int len, struct ITCPCB *tcp, void *user);
	static int KcpOutput(const char *buf, int len, struct IKCPCB *kcp, void *user);
	static int NackOutput(ProtocolPacket *packet, void *user);
	static void KcpLog(const char *log, struct IKCPCB *kcp, void *user);
	static void NackLog(const char *log, void *user);

public:
	PacketList output;				// 需要发送的包列表
	System::SockAddress origin;		// 原始目标地址：最开始Connect的地址
	System::SockAddress remote;		// 当前目标地址：初始化为origin，或调整IP后的地址
	std::string ident;				// 标志
	std::string token;				// 秘钥
	int flags;						// 给外面用的标志
	IUINT32 user;					// 给外面用的标签
	bool deadmark;					// 死亡标记
	IUINT32 deadcode;				// 死亡编码	
	std::list<Session*>::iterator it;	// 自己的迭代器

protected:
	IUINT32 _hid;					// server 端的索引编号
	IUINT32 _conv;					// 会话验证码
	IUINT32 _tscreate;				// 开始的时间
	IUINT32 _tsactive;				// 最近活动时间
	IUINT8 _mask;					// 加密序列
	IUINT32 _feature_local;			// 本地特性
	IUINT32 _feature_remote;		// 远程特性
	int _state;						// 状态
	int _nodelay;					// 是否快速模式
	int _mtu;						// 最大传输单元
	int _mss;						// 最大数据单元
	bool _connect;					// 状态
	bool _ping;						// 是否在 ping
	PacketList _events;				// 接收到的消息
	TimeRto _rto;					// 接收超时
	IUINT32 _current;				// 当前时间
	int _nout_kcp;					// KCP输出包技术
	Timeout _timeout;				// 连接计时器
	Timeout _shutdown;				// 关闭计时器
	Transmission *_transmission;	// 扩展协议
	itcpcb *_tcp;					// 原始TCP协议
	ikcpcb *_kcp;					// 原始KCP协议
	RequestRepeat *_repeat;			// 拉包协议
	Combinator *_combinator;		// 合并数据
	bool _updated;					// 是否更新过
	Trace *trace;					// 日志输出器
};



//---------------------------------------------------------------------
// 协议配置
//---------------------------------------------------------------------
#define QUICKNET_OPT_NODELAY		0x1001	// 调用 send后立马发送：0、1
#define QUICKNET_OPT_KCP_INTERVAL	0x1002	// 设置 KCP的内部时钟
#define QUICKNET_OPT_KCP_NODELAY	0x1003	// 设置 KCP是否 nodelay
#define QUICKNET_OPT_KCP_RESEND		0x1004	// 设置 KCP是否启用 resend
#define QUICKNET_OPT_KCP_NC			0x1005	// 设置 KCP是否关闭流控
#define QUICKNET_OPT_KCP_SNDWND		0x1006	// 设置 KCP的发送
#define QUICKNET_OPT_KCP_RCVWND		0x1007	// 设置 KCP的接收窗口
#define QUICKNET_OPT_KCP_MINRTO		0x1008	// 设置 KCP的最小RTO
#define QUICKNET_OPT_KCP_LOG		0x1009	// 设置 KCP的日志
#define QUICKNET_OPT_KCP_XMIT		0x100A	// 设置 xmit sum in kcp send buffer
#define QUICKNET_OPT_KCP_DEAD_LINK	0x100B	// 设置 dead_link . 每个segment重发次xmit数超过此，则为dead_link.
#define QUICKNET_OPT_KCP_STREAM		0x100C	// 设置 使用流模式
#define QUICKNET_OPT_TCP_NODELAY	0x1010	// 设置 TCP的 NODELAY模式：0,1
#define QUICKNET_OPT_TCP_BUFSIZE	0x1011	// 设置 TCP的 缓存大小
#define QUICKNET_OPT_NACK_BUFSIZE	0x1012	// 设置 拉包缓存大小 （单位：包个数）
#define QUICKNET_OPT_NACK_LOG		0x1013	// 设置 拉包日志
#define QUICKNET_OPT_NACK_SKIPSIZE	0x1014	// 设置 拉包忽略的包个数
#define QUICKNET_OPT_KCP_RTO		0x1015	// 读取：KCP的 RTO
#define QUICKNET_OPT_KCP_WAITSND	0x1016	// 读取：KCP的待发送数据大小
#define QUICKNET_OPT_KCP_SNDPKT		0x1017	// 读取：KCP的总发送数据包
#define QUICKNET_OPT_KCP_RTT		0x1018	// 读取：KCP的RTT
#define QUICKNET_OPT_KCP_OUTWND		0x1019	// 读取：KCP的不在发送窗口中的数据包个数
#define QUICKNET_OPT_KCP_WNDSND		0x1020	// 读取：KCP的当前窗口包数量
#define QUICKNET_OPT_COMBINE_LIMIT	0x1022	// 设置：合包的限制（默认900）
#define QUICKNET_OPT_COMBINE_PERIOD	0x1023	// 设置：合包的周期（默认20）
#define QUICKNET_OPT_FEATURE_LOC	0x1084	// 设置：本地特性
#define QUICKNET_OPT_FEATURE_RMT	0x1085	// 设置：远程特性

#define QUICKNET_FEATURES			0x3		// 1 + 2	

#define QUICKNET_FEATURE_COMBINE_RECV	1
#define QUICKNET_FEATURE_COMBINE_SEND	2


//---------------------------------------------------------------------
// SessionDict
// 用于服务端以整数 hid的方式管理所有 Session的容器
//---------------------------------------------------------------------
class SessionDict
{
public:
	inline SessionDict();
	inline virtual ~SessionDict();

	// 根据 hid取得 session，不存在返回NULL
	inline Session* GetSession(IUINT32 hid);
	inline const Session* GetSession(IUINT32 hid) const;

	// 根据 hid取得 session，不存在返回NULL
	inline Session* operator[](IUINT32 hid);
	inline const Session* operator[](IUINT32 hid) const;

	// 新建 session，返回新 session的 hid
	inline IUINT32 NewSession(IUINT32 conv);
	inline bool DelSession(IUINT32 hid);

	// 迭代器
	inline IUINT32 First() const;
	inline IUINT32 Next(IUINT32 hid) const;
	inline int Count() const;
	inline void Clear();

	typedef std::list<Session*> SessionList;
	inline SessionList::const_iterator Begin() const;
	inline SessionList::const_iterator End() const;

protected:
	IUINT32 _hiword;
	SessionList _list;
	System::MemNode _nodes;
};


// 初始化 高位字节以及会话编号
inline SessionDict::SessionDict() {
	_hiword = 1;
}

inline SessionDict::~SessionDict() {
	Clear();
}

// 根据 hid取得 session，不存在返回NULL
inline Session* SessionDict::GetSession(IUINT32 hid) {
	IUINT32 index = ((IUINT32)hid) & 0x3fff;
	//if (hid < 0) return NULL;
	if ((IINT32)index >= _nodes.node_max()) return NULL;
	Session *session = (Session*)_nodes[index];
	if (session == NULL) return NULL;
	if (session->hid() != (IUINT32)hid) return NULL;
	return session;
}

// 根据 hid取得 session，不存在返回NULL
inline const Session* SessionDict::GetSession(IUINT32 hid) const {
	IUINT32 index = ((IUINT32)hid) & 0x3fff;
	//if (hid < 0) return NULL;
	if ((IINT32)index >= _nodes.node_max()) return NULL;
	const Session *session = (const Session*)_nodes[index];
	if (session == NULL) return NULL;
	if (session->hid() != (IUINT32)hid) return NULL;
	return session;
}

// 根据 hid取得 session，不存在返回NULL
inline Session* SessionDict::operator[](IUINT32 hid) {
	return GetSession(hid);
}

// 根据 hid取得 session，不存在返回NULL
inline const Session* SessionDict::operator[](IUINT32 hid) const {
	return GetSession(hid);
}

// 新建一个 Session（自动生成 hid并返回）
inline IUINT32 SessionDict::NewSession(IUINT32 conv) {
	if (_nodes.size() >= 0x3fff) return 0;
	ilong id = _nodes.new_node();
	if (id < 0) return 0;
	if (id >= 0x3fff) {
		SYSTEM_THROW("SessionDict::NewSession error new id", 10000);
		return 0;
	}
	IUINT32 hid = (IUINT32)id;
	hid |= _hiword << 14;
	_hiword++;
	if (_hiword > 0x1fff) {
		_hiword = 1;
	}
	Session *session = new Session(hid, conv);
	if (session == NULL) {
		_nodes.delete_node(id);
		SYSTEM_THROW("SessionDict::NewSession error new session", 10001);
		return 0;
	}
	_nodes[id] = session;
	session->it = _list.insert(_list.end(), session);
	return hid;
}

// 删除一个 Session
inline bool SessionDict::DelSession(IUINT32 hid) {
	IUINT32 index = ((IUINT32)hid) & 0x3fff;
	//if (hid < 0) return false;
	if ((IUINT32)index >= (IUINT32)_nodes.node_max()) return false;
	Session *session = (Session*)_nodes[index];
	if (session == NULL) return false;
	if (session->hid() != (IUINT32)hid) return false;
	_nodes[index] = NULL;
	_list.erase(session->it);
	session->it = _list.end();
	delete session;
	_nodes.delete_node(index);
	return true;
}

// 取得第一个 hid
inline IUINT32 SessionDict::First() const {
	ilong id = _nodes.head();
	if (id < 0) return 0;
	const Session *session = (const Session*)_nodes[id];
	if (session == NULL) {
		SYSTEM_THROW("SessionDict::First error", 10002);
		return 0;
	}
	return session->hid();
}

// 取得下一个 hid
inline IUINT32 SessionDict::Next(IUINT32 hid) const {
	ilong index = (ilong)(hid & 0x3fff);
	if (index < 0 || (IUINT32)index >= (IUINT32)_nodes.node_max()) return 0;
	const Session *session = (const Session*)_nodes[index];
	if (session == NULL) return 0;
	if (session->hid() != (IUINT32)hid) return 0;
	index = _nodes.next(index);
	if (index < 0) return 0;
	session = (const Session*)_nodes[index];
	if (session == NULL) {
		SYSTEM_THROW("SessionDict::Next error", 10003);
		return 0;
	}
	return session->hid();
}

// 取得数量
inline int SessionDict::Count() const {
	return _nodes.size();
}

// 清空会话
inline void SessionDict::Clear() {
	while (_nodes.size() > 0) {
		IUINT32 hid = First();
		if (hid == 0) {
			SYSTEM_THROW("SessionDict::Clear error", 10004);
			return;
		}
		bool hr = DelSession(hid);
		if (hr == false) {
			SYSTEM_THROW("SessionDict::Clear delete error", 10005);
			return;
		}
	}
}

inline SessionDict::SessionList::const_iterator SessionDict::Begin() const {
	return _list.begin();
}

inline SessionDict::SessionList::const_iterator SessionDict::End() const {
	return _list.end();
}

NAMESPACE_END(QuickNet)



#endif


