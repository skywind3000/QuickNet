//=====================================================================
//
// ProtocolImp.cpp - 传输协议实现模块
// 
// NOTE:
// for more information, please see the readme file
//
//=====================================================================
#include "ProtocolImp.h"
#include "FecTransmission.h"

NAMESPACE_BEGIN(QuickNet)

//=====================================================================
// 通用配置生成
//=====================================================================
static struct { const char *name; int option; } OptionNames[] = {
	{ "session.nodelay",	QUICKNET_OPT_NODELAY },			// SESSION是否有数据立马发送？
	{ "kcp.interval",		QUICKNET_OPT_KCP_INTERVAL },	// 设置 KCP的内部时钟
	{ "kcp.nodelay",		QUICKNET_OPT_KCP_NODELAY },		// 设置 KCP是否 nodelay
	{ "kcp.resend",			QUICKNET_OPT_KCP_RESEND },		// 设置 KCP是否启用 resend
	{ "kcp.nc",				QUICKNET_OPT_KCP_NC },			// 设置 KCP是否关闭流控
	{ "kcp.sndwnd",			QUICKNET_OPT_KCP_SNDWND },		// 设置 KCP的发送
	{ "kcp.rcvwnd",			QUICKNET_OPT_KCP_RCVWND },		// 设置 KCP的接收窗口
	{ "kcp.minrto",			QUICKNET_OPT_KCP_MINRTO },		// 设置 KCP的最小超时判断
	{ "kcp.log",			QUICKNET_OPT_KCP_LOG },			// 设置 KCP的日志级别
	{ "kcp.stream",			QUICKNET_OPT_KCP_STREAM },		// 设置 KCP的流模式
	{ "tcp.nodelay",		QUICKNET_OPT_TCP_NODELAY },		// 设置 TCP的 NODELAY模式：0,1
	{ "tcp.bufsize",		QUICKNET_OPT_TCP_BUFSIZE },		// 设置 TCP的 缓存大小
	{ "fec.maxbufsize",		QUICKNET_OPT_FEC_MAXBUFSIZE },
	{ "fec.itemnum",		QUICKNET_OPT_FEC_BUFITEM_NUM },
	{ "fec.maxk",			QUICKNET_OPT_FEC_MAXK },
	{ "fec.enable",			QUICKNET_OPT_FEC_ENABLED },
	{ "fec.sorted",			QUICKNET_OPT_FEC_SORTED },
	{ "fec.lostrate",		QUICKNET_OPT_FEC_LOST_RATE },
	{ "fec.k",				QUICKNET_OPT_FEC_STATIC_K },
	{ "fec.n",				QUICKNET_OPT_FEC_STATIC_N },
	{ "fec.dynamic",		QUICKNET_OPT_FEC_DYNKN },
	{ "nack.bufsize",		QUICKNET_OPT_NACK_BUFSIZE },	// 设置 拉包缓存大小（单位：包个数）
	{ "nack.log",			QUICKNET_OPT_NACK_LOG},
	{ "nack.skipsize",		QUICKNET_OPT_NACK_SKIPSIZE},
	{ "feature.local",		QUICKNET_OPT_FEATURE_LOC },
	{ "combine.limit",		QUICKNET_OPT_COMBINE_LIMIT },
	{ "combine.period",		QUICKNET_OPT_COMBINE_PERIOD },
	{ NULL, 0 },
};

// 解析配置
bool ParseConfig(const char *config, std::vector<int> &options, std::vector<int> &values)
{
	std::string text = config;

	System::StringList names;
	System::StringList datas;
	System::StringConfig(text, names, datas);

	options.clear();
	values.clear();

	for (size_t i = 0; i < names.size(); i++) {
		std::string &name = names[i];
		std::string &data = datas[i];
		System::StringLower(name);
		System::StringLower(data);
		for (int k = 0; ; k++) {
			if (OptionNames[k].name == NULL) {
				//return false;	// 没有找到配置
				break;
			}
			if (name == OptionNames[k].name) {
				int x = 0;
				if (data == "true") x = 1;
				else if (data == "false") x = 0;
				else x = System::String2Int(data);
				options.push_back(OptionNames[k].option);
				values.push_back(x);
				break;
			}
		}
	}

	return true;
}



//=====================================================================
// 协议服务端
//=====================================================================
int FecEnable = 1;

//---------------------------------------------------------------------
// QuickServer
//---------------------------------------------------------------------
QuickServer::QuickServer()
{
	trace = &Trace::Global;
	_manager = new SessionManager(this);
	_manager->SetTrace(trace);
	_network.SetTrace(trace);
	_manager->PacketOutput = PacketOutput;
	if (FecEnable) {
		_manager->SetTransmission( CreateFecTransmission );
	}
	_interval = 20;
	_current = iclock();
	_slap = _current + _interval;
}


QuickServer::~QuickServer()
{
	if (_manager) {
		delete _manager;
		_manager = NULL;
	}
}

// 设置日志
void QuickServer::SetTrace(Trace *trace)
{
	if (trace) {
		this->trace = trace;
		_manager->SetTrace(trace);
		_network.SetTrace(trace);
	}
}

// 静态函数：网络数据包输出
void QuickServer::PacketOutput(ProtocolPacket *packet, const System::SockAddress &remote, void *user)
{
	QuickServer *server = (QuickServer*)user;
	server->_network.SendPacket(packet, remote);
}

// 更新
void QuickServer::Update(bool Force)
{
	_current = iclock();
	if (_interval < 1) {
		_interval = 1;
	}
	if (Force) {
		_slap = _current;
	}
	if (itimediff(_current, _slap) >= 0) {
		if (itimediff(_current, _slap) > 100000) _slap = _current;
		while (itimediff(_current, _slap) < 0) _slap += _interval;
		UpdateInterval();
	}
}


//---------------------------------------------------------------------
// 定时更新
//---------------------------------------------------------------------
void QuickServer::UpdateInterval()
{
	// 再次更新时钟
	_current = iclock();

	// 设置时钟（优化，仅仅设置时钟）
	_manager->Update(_current, false, false);

	// 远端地址
	System::SockAddress remote;
	
	// 传入网络包
	while (1) {
		ProtocolPacket *packet = _network.RecvPacket(remote);
		if (packet == NULL) break;
		if (packet->cmd == QUICKNET_CMD_HELLO) {
			ProtocolPacket *newpacket = new ProtocolPacket(packet->size() + _uuid.size());
			newpacket->push_tail(packet->data(), packet->size());
			newpacket->push_tail(_uuid.c_str(), _uuid.size());
			newpacket->cmd = QUICKNET_CMD_HBACK;
			delete packet;
			_network.SendPacket(newpacket, remote);
		}
		else {
			_manager->PacketInput(packet, remote);
		}
	}

	// 再次更新时钟
	_current = iclock();

	// 扫描更新
	_manager->Update(_current, true, true);

	// 再次更新时钟
	_current = iclock();

	// 更新统计
	_network.StatisticUpdate(_stat);
}


//---------------------------------------------------------------------
// 启动服务
//---------------------------------------------------------------------
bool QuickServer::StartService(int port, const char *ip)
{
	_network.Close();
	if (_network.Open(port, ip) == false) {
		trace->out(TRACE_SERVER, "[SERVER] service failed on binding to port %d", port);
		return false;
	}
	_manager->Shutdown();
	trace->out(TRACE_SERVER, "[SERVER] service start up at port %d", port);
	_port = _port + 0;
	Update(true);
	_network.SetSocketBuffer(1024 * 1024 * 2, 1024 * 1024 * 2);

	MakeUUID(port);
	
	return true;
}


//---------------------------------------------------------------------
// 停止服务
//---------------------------------------------------------------------
void QuickServer::Shutdown()
{
	_manager->Shutdown();
	_network.Close();
	trace->out(TRACE_SERVER, "[SERVER] service shutdown");
}


//---------------------------------------------------------------------
// 生成服务器唯一ID
//---------------------------------------------------------------------
void QuickServer::MakeUUID(int port)
{
	std::string uuid;
	char text[100];
	isocket_update_address(0);
	if (ihost_addr_num > 0) {
		for (int i = 0; i < 4 && i < ihost_addr_num; i++) {
			uuid += ihost_ipstr[i];
			uuid += ":";
		}
	}
	else {
		uuid = "127.0.0.1:";
	}
	IUINT64 ts = iclockrt();
	iulltoa(ts, text, 10);
	uuid += text;
	iltoa(port, text, 10);
	uuid += ":";
	uuid += text;
	_uuid = uuid;
}


//---------------------------------------------------------------------
// 取得消息，成功返回消息长度，返回-1为没有消息，-2为长度错误
// 如果 event, wparam, lparam, 任意为 NULL则返回消息长度
//---------------------------------------------------------------------
int QuickServer::Read(int *event, IUINT32 *wparam, IUINT32 *lparam, void *data, int size)
{
	return _manager->ReadEvent(event, wparam, lparam, data, size);
}


// 发送数据
bool QuickServer::Send(IINT32 hid, int protocol, const void *data, int size, int limit)
{
	return _manager->SessionSend(hid, protocol, data, size, limit);
}

// 断开连接
bool QuickServer::Close(IINT32 hid, int code)
{
	return _manager->SessionClose(hid, code);
}

// 设置超时
// idle是空闲超时，-1为不设置，0为取消空闲超时，>0为毫秒时间，超过这个时间收不到包的客户端断开
// connect连接超时，-1为不设置，0为取消连接超时，>0为毫秒时间，超过这个时间没连接成功的客户端断开
void QuickServer::SetTimeOut(int idle, int connect)
{
	_manager->SetTimeOut(idle, connect);
}


// 设置周期
void QuickServer::SetInterval(int interval)
{
	_manager->SetInterval(interval);
	_interval = interval;
}

// 配置
int QuickServer::Option(IUINT32 hid, int option, int value)
{
	return _manager->Option(hid, option, value);
}

// 配置
int QuickServer::GetOption(IUINT32 hid, int option)
{
    if (_manager == NULL)
    {
        return -1;
    }
    return _manager->GetOption(hid, option);
}

// 安装传输层协议
void QuickServer::SetTransmission(Transmission::Factory factory)
{
	_manager->SetTransmission(factory);
}

// 取得本地地址
void QuickServer::LocalAddress(System::SockAddress &local)
{
	_network.LocalAddress(local);
}


// 字符串配置
int QuickServer::Option(IUINT32 hid, const char *options)
{
	std::vector<int> items;
	std::vector<int> values;
	int retval = 0;

	if (ParseConfig(options, items, values) == false) {
		return -1;
	}

	for (size_t i = 0; i < items.size(); i++) {
		int hr = Option(hid, items[i], values[i]);
		if (hr != 0) retval = -2;
	}

	return retval;
}


// 取得某 HID的状态：0关闭，1连接1，2连接2，3连接等待，4建立连接，5关闭等待。-1为错误 hid
int QuickServer::GetState(IUINT32 hid) const
{
	return _manager->GetState(hid);
}

// 广播一群人，返回成功个数
int QuickServer::GroupCast(const IUINT32 *hid, int count, int protocol, const void *data, int size, int limit)
{
	return _manager->GroupCast(hid, count, protocol, data, size, limit);
}

// 广播所有人，返回成功个数
int QuickServer::Broadcast(int protocol, const void *data, int size, int limit)
{
	return _manager->Broadcast(protocol, data, size, limit);
}

// 取得统计信息
void QuickServer::Statistic(ProtocolUdp::Statistic &stat)
{
	stat = _stat;
}


// 全局掩码
void QuickServer::SetGlobalMask(IUINT32 mask)
{
	_network.SetGlobalMask(mask);
}

// 取得KCP等待发送的数据
int QuickServer::GetPending(IUINT32 hid, int what) const
{
	return _manager->GetPending(hid, what);
}


//=====================================================================
// 协议客户端
//=====================================================================
static const IUINT32 CLIENT_PING_INTERVAL = 50000;		// 50秒启动一次 PING


//---------------------------------------------------------------------
// QuickClient
//---------------------------------------------------------------------
QuickClient::QuickClient()
{
	System::CriticalScope scope(_lock);
    trace = &Trace::Global;
    _current = iclock();
    _session = new Session(0, iclock());
    _factory = NULL;
    if (FecEnable) {
        _factory = CreateFecTransmission;
    }
    _started = false;
    _ping_timer.stop();
    _network.SetTrace(trace);
    _session->SetTrace(trace);
    _keepalive = CLIENT_PING_INTERVAL;
	_time_max_connect = SESSION_MAX_CONN_TIME;
	_time_max_idle    = SESSION_MAX_IDLE_TIME;
	_mask = 0;
}

QuickClient::~QuickClient()
{
 	System::CriticalScope scope(_lock);
	_network.Close();
	_started = false;
	if (_session) {
		delete _session;
		_session = NULL;
	}
}

void QuickClient::SetTrace(Trace *trace)
{
	System::CriticalScope scope(_lock);
	if (trace) {
		this->trace = trace;
		_session->SetTrace(trace);
		_network.SetTrace(trace);
	}
}

// 设置超时
// idle是空闲超时，-1为不设置，0为取消空闲超时，>0为毫秒时间，超过这个时间收不到包的客户端断开
// connect连接超时，-1为不设置，0为取消连接超时，>0为毫秒时间，超过这个时间没连接成功的客户端断开
void QuickClient::SetTimeOut(int idle, int connect)
{
	System::CriticalScope scope(_lock);
	if (idle >= 0) {
		_time_max_idle = idle;
	}
	if (connect > 0) {
		_time_max_connect = connect;
	}
}

bool QuickClient::Connect(const char *ip, int port)
{
	System::CriticalScope scope(_lock);
	_session->Disconnect();
	_network.Close();

	if (_network.Open(0, 0) == false) {
		return false;
	}

	_current = iclock();

	_network.SetSocketBuffer(16384, 16384);
	_network.SetSocketBuffer(8192, 8192);

	delete _session;
	_session = new Session(1, 2);

	if (_factory) 
    {
		_session->TransmissionInstall(_factory);
	}

	_session->Update(_current);
	System::SockAddress remote(ip, port);

	if (_session->Connect(remote, iclock()) == false) {
		_network.Close();
		return false;
	}

	_ping_timer.start(_current, _keepalive, 100, true);

	_target = remote;
	_started = true;

	trace->out(TRACE_CLIENT, "[CLIENT] connecting to %s:%d", ip, port);
//	printf("hid=%lx conv=%lu\n", _session->hid(), _session->conv());

	return true;
}

void QuickClient::Close()
{
	System::CriticalScope scope(_lock);
	_session->Disconnect();
	trace->out(TRACE_CLIENT, "[CLIENT] disconnect");
	_started = false;
}

void QuickClient::CheckOutput()
{
	// 扫描需要发送出去的数据包
	for(PacketList::iterator it = _session->output.begin(); it != _session->output.end(); it++)
	{
		ProtocolPacket *packet = *it;
		if (packet == NULL)
		{
			continue;
		}
		_network.SendPacket(packet, _session->remote);
		if (trace != NULL && trace->available(TRACE_MGR_PACKET)) {
            int size = packet->size();
			trace->out(TRACE_MGR_PACKET, "[CLIENT] [PACKET] output %d bytes", size);
		}
	}
	_session->output.clear();
}

// 发送数据
bool QuickClient::Send(int protocol, const void *data, int size, int limit)
{
	System::CriticalScope scope(_lock);
	return _session->Send(protocol, data, size, limit);
}

// 接收数据
int QuickClient::Recv(int *protocol, void *data, int size)
{
	System::CriticalScope scope(_lock);
	return _session->Recv(protocol, data, size);
}


//---------------------------------------------------------------------
// 客户端更新
//---------------------------------------------------------------------
void QuickClient::Update()
{
	std::string strTransferIP = _pinger.GetTransferHostIP();
	unsigned int nTransferPort = _pinger.GetTransferHostPort();
	if (strTransferIP.size()>0 && nTransferPort >0)
	{
        QuickNet::Trace::Global.out(TRACE_ERROR,
            "[IP_TUNEL_ROUTER], [Success]=Transfered, [Transfer-Host]=%s,[Transfer-Port]=%d",  strTransferIP.c_str(), nTransferPort);

		SwitchTargetAddress(strTransferIP.c_str(), nTransferPort);
        _pinger.ResetTransferHost();
	}

	System::CriticalScope scope(_lock);

	if (_session == NULL || _session->deadmark)
	{
		return;
	}
	_current = iclock();
	_session->Update(_current, false);

	while (1) {
		System::SockAddress remote;
		
		// 从网络接收 UDP
		ProtocolPacket *packet = _network.RecvPacket(remote);
		if (packet == NULL) break;

		if (packet->cmd == QUICKNET_CMD_CHACK) {	// 如果是更改 IP结果
			if (remote == _target) {
				// 解析出第二层协议
				packet->conv = packet->pop_head_uint32();
				packet->hid = packet->pop_head_uint32();
				if (packet->hid == _session->hid() && packet->conv == _session->conv()) {
					if (_session->state() == QUICK_SESSION_STATE_ESTAB) {
						std::string token;
						token.assign(packet->data(), (size_t)packet->size());
						if (token == _session->token) {
							_session->remote = _target;
						}
					}
				}
				_switch_ip.stop();
			}
		}
		else if (remote == _session->remote || remote == _target) {	// 如果是我们的目标地址
			if (packet->size() >= 8) {

				// 解析出第二层协议
				packet->conv = packet->pop_head_uint32();
				packet->hid = packet->pop_head_uint32();

				// 输入 Session
				_session->PacketInput(packet);
				packet = NULL;
			}
			else {
				if (trace->available(TRACE_WARNING)) {
					char text[128];
					trace->out(TRACE_WARNING, "[WARNING] error packet from %s", remote.string(text));
				}
			}
		}
		else {
		}

		if (packet) {
			delete packet;
		}
	}

	if (_session->IsConnecting() && _session->LifeTime(_current) >= (IINT32)_time_max_connect)
	{
		_session->deadmark = true;		// 设置为死亡
		_session->deadcode = QNET_CODE_CONNECT_FAIL;		// 断开编码
		if (trace) {
			trace->out(TRACE_SESSION, "[SESSION] session failed to connect");
		}
	}

	// 判断超时：空闲时间
	// 如果空闲时间限制生效 (<=0为不判断)
	if (_session->state() == QUICK_SESSION_STATE_ESTAB && _session->IdleTime(_current) >= (IINT32)_time_max_idle) 
	{
		_session->IdleTime(_current);
		_session->deadmark = true;		// 设置为死亡
		_session->deadcode = QNET_CODE_TIMEOUT;		// 断开编码
		if (trace) {
			trace->out(TRACE_SESSION, "[SESSION] session lost connection, ping timeout [hid]=%d", _session->hid());
		}
	}

	_session->Update(_current, true);

	// 计算是否需要发起一次 PING或者改变 IP的请求
	if (_session->state() == QUICK_SESSION_STATE_ESTAB) {
		if (_ping_timer.check(_current)) {
			_session->RemotePing();
		}
		if (_switch_ip.check(_current)) {
			ProtocolPacket *packet = new ProtocolPacket(_session->token.size());
			packet->push_tail(_session->token.c_str(), _session->token.size());
			packet->push_head_uint32(_session->hid());
			packet->push_head_uint32(_session->conv());
			packet->mask = _mask++;
			packet->cmd = QUICKNET_CMD_CHGIP;
			_network.SendPacket(packet, _target);
			//printf("send switch to %s, token='%s'\n", _target.string(NULL), _session->token.c_str());
		}
	}

	// 计算是否连接应该断开
	if (_session->state() != QUICK_SESSION_STATE_CLOSED && _session->deadmark) {
		IUINT32 hid = _session->hid();
		_session->Disconnect();
		if (trace) {
			trace->out(TRACE_SESSION, "[SESSION] session closed, [hid]=%d", hid);
		}
	}

	// 更新数据包
	CheckOutput();

	// 更新统计
	_network.StatisticUpdate(_stat);
}


// 设置保活，单位毫秒，小于零为默认值
void QuickClient::Keepalive(int interval)
{
	System::CriticalScope scope(_lock);

	_keepalive = (interval <= 0)? CLIENT_PING_INTERVAL : interval;
	if (_session->state() == QUICK_SESSION_STATE_ESTAB) {
		_ping_timer.stop();
		_ping_timer.start(_current, _keepalive, 100, true);
	}
}

// 改变传输层的目标 ip:port地址，需要连接成功后调用
bool QuickClient::SwitchTargetAddress(const char *ip, int port)
{
	System::CriticalScope scope(_lock);
	if (_session->state() != QUICK_SESSION_STATE_ESTAB) return false;
	_target.set(ip, port);
	_session->remote = _session->origin;
	_switch_ip.stop();
	_switch_ip.start(_current, 200, 150, true);
	return true;
}

// 取得目标地址，如果传入地址相同，返回真，否则返回假
bool QuickClient::GetTargetAddress(System::SockAddress &address) const
{
	System::CriticalScope scope(_lock);
	if (address == _session->remote) return true;
	address = _session->remote;
	return false;
}

// 安装传输层协议
void QuickClient::SetTransmission(Transmission::Factory factory)
{
	System::CriticalScope scope(_lock);
	_factory = factory;
}

IUINT32 QuickClient::GetHid() const
{
	System::CriticalScope scope(_lock);
    IUINT32 nRet = IUINT32(-1);
    if (_session != NULL)
    {
        nRet = _session->hid();
    }
    return nRet;
}

// 配置
int QuickClient::Option(int option, int value)
{
	System::CriticalScope scope(_lock);
	return _session->Option(option, value);
}


int QuickClient::GetOption(int option) const
{
	System::CriticalScope scope(_lock);
    return _session->GetOption(option);

}


// 字符串配置
int QuickClient::Option(const char *options)
{
	std::vector<int> items;
	std::vector<int> values;
	int retval = 0;

	if (ParseConfig(options, items, values) == false) {
		return -1;
	}

	for (int i = 0; i < (int)items.size(); i++) {
		int hr = Option(items[i], values[i]);
		if (hr != 0) retval = -2;
	}

	return retval;
}


// 取得状态：0关闭，1连接1，2连接2，3连接等待，4建立连接，5关闭等待。
int QuickClient::GetState() const
{
	System::CriticalScope scope(_lock);
	return _session->state();
}

// 是否连接：0为没有连接，1为已经连接
int QuickClient::IsConnected() const
{
	System::CriticalScope scope(_lock);
	if (_session->state() == QUICK_SESSION_STATE_ESTAB) return 1;
	return 0;
}

// 取得统计信息
void QuickClient::Statistic(ProtocolUdp::Statistic &stat)
{
	stat = _stat;
}

// 取得 RTT
int QuickClient::GetRtt() const
{
	System::CriticalScope scope(_lock);
	return _session->GetRtt();
}


void QuickClient::GetNACKStatInfo(int* packets, int *pull, int* pullpkts, int *lost, int *pulltimeout, int *skip, int *totalskippkt)
{
	System::CriticalScope scope(_lock);
	return _session->GetNACKStatInfo(packets, pull, pullpkts, lost, pulltimeout, skip, totalskippkt);
}


NePinger& QuickClient::GetPingRouter()
{
    return _pinger;
}

// 全局掩码
void QuickClient::SetGlobalMask(IUINT32 mask)
{
	_network.SetGlobalMask(mask);
}


// 取得待发送数据
int QuickClient::GetPending(int what) const
{
	return _session->GetPending(what);
}


NAMESPACE_END(QuickNet)


