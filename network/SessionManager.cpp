//=====================================================================
//
// SessionManager.cpp - 会话管理模块
// 
// NOTE:
// for more information, please see the readme file
//
//=====================================================================
#include "SessionManager.h"

NAMESPACE_BEGIN(QuickNet)




//---------------------------------------------------------------------
// 会话服务端
//---------------------------------------------------------------------
SessionManager::SessionManager(void *user)
{
	_updated = false;
	_current = 0;
	_user = user;
	_nlisten = QUICKNET_MAX_BACKLOG;
	_interval = 25;
	trace = &Trace::Global;
	_time_max_connect = SESSION_MAX_CONN_TIME;		// 默认最长连接时间
	_time_max_idle = SESSION_MAX_IDLE_TIME;			// 默认最长空闲时间
	PacketOutput = NULL;
	_factory = NULL;
}

SessionManager::~SessionManager()
{
	Shutdown();
}

void SessionManager::SetTrace(Trace *trace)
{
	if (trace) {
		this->trace = trace;
	}
}

// 关闭服务
void SessionManager::Shutdown()
{
	ProtocolUdp::ClearPacketList(_events);
	_listens.clear();
	_sessions.Clear();
}

// output to udp
void SessionManager::Output(ProtocolPacket *packet, const System::SockAddress &remote) 
{
	if (PacketOutput) {
		PacketOutput(packet, remote, _user);
	}	else {
		delete packet;
	}
}

// session output
int SessionManager::SessionForward(Session *session, long limit) 
{
	int count = 0;
	while (!session->output.empty()) {
		PacketList::iterator it = session->output.begin();
		ProtocolPacket *packet = *it;
		session->output.erase(it);
		Output(packet, session->remote);
		count++;
		if (limit > 0 && count >= limit) break;
	}
	return count;
}


// 取得消息
ProtocolPacket *SessionManager::GetEvent(bool peek)
{
	PacketList::iterator it = _events.begin();
	if (it == _events.end()) return NULL;
	ProtocolPacket *packet = *it;
	if (peek == false) {
		_events.erase(it);
	}
	return packet;
}


// 取得消息 直接方式，成功返回消息长度，返回-1代表没有消息，-2代表长度错误，
// 如果 event, wparam, lparam 任意为 NULL 则返回消息长度
int SessionManager::ReadEvent(int *event, IUINT32 *wparam, IUINT32 *lparam, void *data, int size)
{
	PacketList::iterator it = _events.begin();
	if (it == _events.end()) return -1;

	ProtocolPacket *packet = *it;

	if (event == NULL || wparam == NULL || lparam == NULL) {
		return packet->size();
	}

	if (packet->size() > size) return -2;
	size = packet->size();

	if (data != NULL) {
		packet->pop_tail(data, size);
	}

	*event = packet->event;
	*wparam = packet->wparam;
	*lparam = packet->lparam;

	_events.erase(it);
	delete packet;

	return size;
}


// 发送消息
void SessionManager::PostEvent(ProtocolPacket *packet)
{
	_events.push_back(packet);
	if (trace->available(TRACE_MGR_PACKET)) {
		trace->out(TRACE_MGR_PACKET, "[EVENT] event=%d wparam=%lx lparam=%lx size=%d", 
			(int)packet->event, packet->wparam, packet->lparam, (int)packet->size());
	}
}

// 发送消息
void SessionManager::PostEvent(int event, IUINT32 wparam, IUINT32 lparam, const void *data, int size)
{
	ProtocolPacket *packet = new ProtocolPacket(size);

	if (packet == NULL) {
		NETWORK_THROW("can not create event packet", 10000);
		return;
	}

	packet->event = event;
	packet->wparam = wparam;
	packet->lparam = lparam;

	if (size && data) {
		packet->push_tail(data, size);
	}

	PostEvent(packet);
}


// 刚刚输入数据到 session以后调用
void SessionManager::FetchEvents(Session *session)
{
	while (1) {
		ProtocolPacket *packet = session->Recv();
		if (packet == NULL) break;
		packet->event = QUICKNET_EVENT_DATA;
		packet->wparam = packet->hid;
		packet->lparam = packet->protocol;
		PostEvent(packet);
	}
}


//---------------------------------------------------------------------
// 更新状态
//---------------------------------------------------------------------
void SessionManager::Update(IUINT32 current, bool force, bool flush)
{
	_current = current;
	if (_updated == false) {
		_updated = true;
		_slap = _current;
	}
	if (_interval < 1) {
		_interval = 1;
	}
	if (force) {
		_slap = _current;
	}
	if (_current >= _slap) {
		int step = ((_current - _slap) + _interval) / _interval;
		_slap += step * _interval;
		while (itimediff(_slap, _current) < 0) _slap += _interval;
		UpdateInterval(flush);
	}
}

//---------------------------------------------------------------------
// 按照时钟节拍更新
//---------------------------------------------------------------------
void SessionManager::UpdateInterval(bool flush)
{
	_remove.clear();

	if (flush == false) {
		SessionDict::SessionList::const_iterator st;
		for (st = _sessions.Begin(); st != _sessions.End(); st++) {
			Session *session = *st;
			assert(session);
			session->Update(_current, false);
		}
		return;
	}

	ListenBacklog::const_iterator it = _listens.begin();

	// 扫面监听超时
	for (; it != _listens.end(); it++) {
		IUINT32 hid = it->second;
		Session *session = _sessions[hid];
		if (session) {
			if (session->LifeTime(_current) >= (IINT32)_time_max_connect) {
				_remove.push_back(hid);
			}
		}
		else {
			_remove.push_back(hid);
		}
	}

	// 更新所有链接
	SessionDict::SessionList::const_iterator st;
	for (st = _sessions.Begin(); st != _sessions.End(); st++) {
		Session *session = *st;

		assert(session);

		// 更新状态
		session->Update(_current, true);
		SessionForward(session);

		// 接收所有协议的包
		FetchEvents(session);

		// 判断超时：空闲时间
		if (_time_max_idle > 0) {	// 如果空闲时间限制生效 (<=0为不判断)
			if (session->IdleTime(_current) >= (IINT32)_time_max_idle) {
				session->deadmark = true;		// 设置为死亡
				session->deadcode = QNET_CODE_TIMEOUT;		// 断开编码
			}
		}

		// 判断是否关闭连接
		if (session->state() == QUICK_SESSION_STATE_CLOSED || session->deadmark) {
			_remove.push_back(session->hid());
		}
	}

	// 删除该删除的节点
	for (int i = 0; i < (int)_remove.size(); i++) {
		IUINT32 hid = _remove[i];
		Session *session = _sessions[hid];
		if (session) {
			OnDisconnected(session, session->deadcode);
		}
		_sessions.DelSession(hid);
	}

	_remove.clear();
}


//---------------------------------------------------------------------
// 关闭连接
//---------------------------------------------------------------------
bool SessionManager::SessionClose(IUINT32 hid, IUINT32 code)
{
	Session *session = _sessions[hid];
	if (session) {
		if (session->deadmark == false) {
			session->deadmark = true;
			session->deadcode = code;
		}
		return true;
	}
	return false;
}


//---------------------------------------------------------------------
// 发送数据
//---------------------------------------------------------------------
bool SessionManager::SessionSend(IUINT32 hid, ProtocolPacket *packet, int limit)
{
	Session *session = _sessions[hid];
	if (session == NULL) return false;
	bool hr = session->Send(packet, limit);
	SessionForward(session);
	return hr;
}


// 发送数据
bool SessionManager::SessionSend(IUINT32 hid, int protocol, const void *data, int size, int limit)
{
	Session *session = _sessions[hid];
	if (session == NULL) return false;
	bool hr = session->Send(protocol, data, size, limit);
	SessionForward(session);
	return hr;
}


//---------------------------------------------------------------------
// 输入第一层协议的协议包，解码层第二层协议，传递给其他
//---------------------------------------------------------------------
void SessionManager::PacketInput(ProtocolPacket *packet, const System::SockAddress &remote)
{
	char text[64];

	if (packet->size() < 8 || _updated == false) {
		if (trace->available(TRACE_WARNING)) {
			trace->out(TRACE_WARNING, "[WARNING] error packet from %s", remote.string(text));
		}
		delete packet;
		return;
	}

	packet->conv = packet->pop_head_uint32();
	packet->hid = packet->pop_head_uint32();

	if (trace->available(TRACE_MGR_PACKET)) {
		trace->out(TRACE_MGR_PACKET, "[MGR] packet-in: cmd=%d protocol=%d hid=%lx size=%d", 
			(int)packet->cmd, (int)packet->protocol, packet->hid, (int)packet->size());
	}

	if (packet->cmd == QUICKNET_CMD_SYN1) {			// 连接1
		HandleSyn1(packet, remote);
		packet = NULL;
	}
	else if (packet->cmd == QUICKNET_CMD_SYN2) {	// 连接2
		HandleSyn2(packet, remote);
		packet = NULL;
	}
	else if (packet->cmd == QUICKNET_CMD_CHGIP) {	// 改变IP
		Session *session = _sessions[packet->hid];
		if (session != NULL) {
			if (packet->conv == session->conv()) {
				int size = (int)packet->size();
				if (size == (int)session->token.size()) {
					if (memcmp(session->token.c_str(), packet->data(), size) == 0) {
						session->remote = remote;
						session->PacketInput(packet);
						packet = NULL;
					}
				}
			}
		}
		if (packet) {
			delete packet;
			packet = NULL;
		}
	}
	else {
		Session *session = _sessions[packet->hid];
		if (session != NULL) {
			if (packet->conv == session->conv()) {
				if (remote == session->remote || remote == session->origin) {
					session->PacketInput(packet);
					packet = NULL;
					SessionForward(session);
					FetchEvents(session);
				}
				else {
					if (trace->available(TRACE_WARNING)) {
						char text2[128];
						trace->out(TRACE_WARNING, "[WARNING] error sockaddr from %s not %s hid=%lx",
							session->remote.string(text2), remote.string(text), packet->hid);
					}
					delete packet;
					packet = NULL;
				}
			}
			else {
				if (trace->available(TRACE_WARNING)) {
					trace->out(TRACE_WARNING, "[WARNING] error conv from %s hid=%lx",
						remote.string(text), packet->hid);
				}
			}
		}

		// 如果没有成功处理网络包
		if (packet != NULL) {
			switch (packet->cmd) {
			case QUICKNET_CMD_FIN:
				packet->cmd = QUICKNET_CMD_FACK;
				packet->push_head_uint32(packet->hid);
				packet->push_head_uint32(packet->conv);
				Output(packet, remote);
				packet = NULL;
				break;

			case QUICKNET_CMD_DATA:
			case QUICKNET_CMD_PING:
				packet->cmd = QUICKNET_CMD_FIN;
				packet->push_head_uint32(packet->hid);
				packet->push_head_uint32(packet->conv);
				Output(packet, remote);
				packet = NULL;
				break;
			}
		}
	}

	if (packet) {
		delete packet;
	}
}


// 计算标识
void SessionManager::GetIdent(const System::SockAddress &remote, IUINT32 conv, std::string &ident)
{
	IUINT32 ip = remote.get_ip();
	IUINT32 port = remote.get_port();
	char head[12];
	iencode32u_lsb(head + 0, ip);
	iencode32u_lsb(head + 4, port);
	iencode32u_lsb(head + 8, conv);
	ident.assign(head, 12);
}

// 处理半连接请求
void SessionManager::HandleSyn1(ProtocolPacket *packet, const System::SockAddress &remote)
{
	std::string ident;
	ListenBacklog::const_iterator it;
	Session *session = NULL;

	// 生成 ident
	GetIdent(remote, packet->conv, ident);
	it = _listens.find(ident);

	// 如果是新连接
	if (it == _listens.end()) {

		// 如果超过最大人数
		if (_sessions.Count() >= QUICKNET_MAX_SESSION) {
			char text[64];
			packet->cmd = QUICKNET_CMD_FIN;
			packet->push_head_uint32(packet->hid);
			packet->push_head_uint32(packet->conv);
			Output(packet, remote);
			trace->out(TRACE_WARNING, "[MGR][WARNING] SYN1 failed: too many sessions from %s", remote.string(text));
			return;
		}

		// 如果大于同时连接数
		if ((int)_listens.size() >= _nlisten) {
			char text[64];
			packet->cmd = QUICKNET_CMD_FIN;
			packet->push_head_uint32(packet->hid);
			packet->push_head_uint32(packet->conv);
			Output(packet, remote);
			trace->out(TRACE_WARNING, "[MGR][WARNING] SYN1 failed: too many listener from %s", remote.string(text));
			return;
		}

		// 新增连接
		IUINT32 hid = _sessions.NewSession(packet->conv);

		if (hid == 0) {
			delete packet;
			NETWORK_THROW("SessionManager::HandleSyn1 error new hid", 11000);
			return;
		}

		// 记录 ident到监听队列
		_listens[ident] = hid;	
		session = _sessions[hid];

		if (session == NULL) {
			delete packet;
			NETWORK_THROW("SessionManager::HandleSyn1 error new session", 11001);
			return;
		}

		session->ident = ident;
		session->flags = 0;
		session->AcceptSyn1(remote);
		session->Update(_current);

		// 如果有协议工厂，则安装扩展协议
		if (_factory) {
			session->TransmissionInstall(_factory);
		}

		if (trace->available(TRACE_MGR_SYN)) {
			char text[64];
			trace->out(TRACE_MGR_SYN, "[MGR] SYN1 new session: hid=%lx from %s",
				hid, remote.string(text));
		}
	}
	else {
		IUINT32 hid = it->second;
		session = _sessions[hid];

		if (session == NULL) {
			delete packet;
			NETWORK_THROW("SessionManager::HandleSyn1 error locate session", 11002);
			return;
		}

		if (trace->available(TRACE_MGR_SYN)) {
			char text[64];
			trace->out(TRACE_MGR_SYN, "[MGR] SYN1 repeat session: hid=%lx from %s",
				hid, remote.string(text));
		}
	}

	if (session == NULL) {
		delete packet;
		return;
	}

	session->Update(_current);
	session->PacketInput(packet);
	SessionForward(session);
}


void SessionManager::HandleSyn2(ProtocolPacket *packet, const System::SockAddress &remote)
{
	IUINT32 hid = packet->hid;
	Session *session = _sessions[hid];
	char text[128];

	if (session == NULL) {
		if (trace->available(TRACE_WARNING)) {
			trace->out(TRACE_WARNING, "[WARNING] syn2 error from %s", remote.string(text));
		}
		delete packet;
		return;
	}

	if (session->remote != remote) {
		if (trace->available(TRACE_WARNING)) {
			char text2[128];
			trace->out(TRACE_WARNING, "[WARNING] error sockaddr from %s not %s hid=%lx",
				session->remote.string(text2), remote.string(text), packet->hid);
		}
		delete packet;
		return;
	}

	int state = session->state();

	session->Update(_current);
	session->PacketInput(packet);
	SessionForward(session);

	// connection established
	if (session->state() == QUICK_SESSION_STATE_ESTAB && state == QUICK_SESSION_STATE_SYNWAIT) {
		OnConnected(session);
	}

}

void SessionManager::OnConnected(Session *session)
{
	char text[128];
	session->flags |= 1;

	ListenBacklog::iterator it = _listens.find(session->ident);

	if (it != _listens.end()) {
		_listens.erase(it);
	}
	else {
		if (trace->available(TRACE_WARNING)) {
			trace->out(TRACE_WARNING, "[WARNING] cannot find in listen backlog hid=%lx", session->hid());
		}
	}

	PostEvent(QUICKNET_EVENT_NEW, session->hid(), session->user, 
		session->remote.address(), sizeof(sockaddr));

	trace->out(TRACE_SESSION, "[SESSION] new session hid=%lx from %s", 
		session->hid(), session->remote.string(text));
}

void SessionManager::OnDisconnected(Session *session, IUINT32 code)
{
	char text[128];

	session->Disconnect();
	SessionForward(session);

	if (session->flags & 1) {
		PostEvent(QUICKNET_EVENT_LEAVE, session->hid(), code, "", 0);
	}	else {
		ListenBacklog::iterator it = _listens.find(session->ident);
		if (it != _listens.end()) {
			_listens.erase(it);
		}
	}

	trace->out(TRACE_SESSION, "[SESSION] close session hid=%lx code=%ld from %s", 
		session->hid(), code, session->remote.string(text));
}


// 设置超时
// idle是空闲超时，-1为不设置，0为取消空闲超时，>0为毫秒时间，超过这个时间收不到包的客户端断开
// connect连接超时，-1为不设置，0为取消连接超时，>0为毫秒时间，超过这个时间没连接成功的客户端断开
void SessionManager::SetTimeOut(int idle, int connect)
{
	if (idle >= 0) {
		_time_max_idle = idle;
	}
	if (connect > 0) {
		_time_max_connect = connect;
	}
}


// 协议配置
int SessionManager::Option(IUINT32 hid, int option, int value)
{
	Session *session = _sessions[hid];
	if (session == NULL) return -1;
	return session->Option(option, value);
}

int SessionManager::GetOption(IUINT32 hid, int option)
{
    Session *session = _sessions[hid];
    if (session == NULL) return -1;
    return session->GetOption(option);
}

// 设置 Interval
void SessionManager::SetInterval(int interval)
{
	_interval = interval;
}

// 设置新协议工厂
void SessionManager::SetTransmission(Transmission::Factory factory)
{
	_factory = factory;
}

void SessionManager::OnData(Session *session)
{
}


// 取得状态：0关闭，1连接1，2连接2，3连接等待，4建立连接，5关闭等待。-1为错误 hid
int SessionManager::GetState(IUINT32 hid) const
{
	const Session *session = _sessions[hid];
	if (session == NULL) return -1;
	return session->state();
}


// 广播 传入 HID列表，进行广播
int SessionManager::GroupCast(const IUINT32 *hid, int count, int protocol, const void *buffer, int size, int limit)
{
	int ok = 0;
	for (int i = 0; i < count; i++) {
		if (SessionSend(hid[i], protocol, buffer, size, limit)) ok++;
	}
	return ok;
}

// 广播所有人
int SessionManager::Broadcast(int protocol, const void *buffer, int size, int limit)
{
	int ok = 0;
	for (IUINT32 hid = _sessions.First(); hid > 0; hid = _sessions.Next(hid)) {
		if (SessionSend(hid, protocol, buffer, size, limit)) ok++;
	}
	return ok;
}

// 取得待发送数据
int SessionManager::GetPending(IUINT32 hid, int what) const
{
	const Session *session = _sessions[hid];
	if (session == NULL) return 0;
	return session->GetPending(what);
}


NAMESPACE_END(QuickNet)


