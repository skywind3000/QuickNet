//=====================================================================
//
// SessionDesc.cpp - 会话模块，一个 Session就是一个链接
// 
// NOTE:
// for more information, please see the readme file
//
//=====================================================================
#include "SessionDesc.h"


NAMESPACE_BEGIN(QuickNet)
//---------------------------------------------------------------------
// 内部常量
//---------------------------------------------------------------------
static const IUINT32 SESSION_CONNECT_INTERVAL	= 300;
static const IUINT32 SESSION_PING_INTERVAL		= 1000;

//---------------------------------------------------------------------
// 链接描述符
//---------------------------------------------------------------------
Session::Session(IUINT32 hid, IUINT32 conv)
{
	_hid = hid;
	_conv = conv;
	_mask = 1;
	_current = 0;
	_tscreate = 0;
	_tsactive = 0;
	_updated = false;
	_connect = false;
	_ping = false;
	flags = 0;
	user = 0;
	_mtu = 1400;
	_mss = _mtu - QUICKNET_SESSION_OVERHEAD;
	deadmark = false;
	_state = QUICK_SESSION_STATE_CLOSED;
	_nodelay = 0;
	_feature_local = 0;
	_feature_remote = 0;
	trace = &Trace::Global;
	_kcp = NULL;
	_tcp = NULL;
	_repeat = NULL;
	_transmission = NULL;
	_combinator = NULL;
	token = "";
	_nout_kcp = 0;
	ProtocolInit();
}

Session::~Session()
{
	ProtocolDestroy();
	ProtocolUdp::ClearPacketList(output);
	ProtocolUdp::ClearPacketList(_events);
	_conv = 0;
	_hid = 0;
}

void Session::SetTrace(Trace *trace)
{
	if (trace) {
		this->trace = trace;
	}
}

void Session::PacketOutput(ProtocolPacket *packet)
{
	packet->hid = _hid;
	packet->conv = _conv;
	packet->mask = _mask++;
	packet->push_head_uint32(packet->hid);
	packet->push_head_uint32(packet->conv);
	output.push_back(packet);
}

// 连接远端地址
bool Session::Connect(System::SockAddress &remote, IUINT32 conv)
{
	if (_state != QUICK_SESSION_STATE_CLOSED) 
		return false;
	if (_updated == false) 
		return false;
	this->origin = remote;
	this->remote = remote;
	this->_conv = conv;
	_state = QUICK_SESSION_STATE_SYN1;
	_timeout.start(_current, SESSION_CONNECT_INTERVAL);
	_connect = false;
	_ping = false;
	ProtocolUdp::ClearPacketList(output);
	ProtocolUdp::ClearPacketList(_events);
	return true;
}

// 断开连接
bool Session::Disconnect()
{
	if (_state == QUICK_SESSION_STATE_CLOSED)
		return false;
	_state = QUICK_SESSION_STATE_FINWAIT;
	SendCommand(QUICKNET_CMD_FIN);
	SendCommand(QUICKNET_CMD_FIN);
	_timeout.start(_current, SESSION_CONNECT_INTERVAL);
	_shutdown.start(_current, 3000, 100, false);
	return true;
}

// 发送协议命令
void Session::SendCommand(int command, const void *ptr, int size)
{
	int length = (size < 0 || ptr == NULL)? 0 : size;
	ProtocolPacket *packet = new ProtocolPacket(length);
	packet->protocol = 0;
	packet->cmd = (IUINT8)command;
	if (length > 0) packet->push_tail(ptr, size);
	PacketOutput(packet);
}

// 接受连接
bool Session::AcceptSyn1(const System::SockAddress &remote)
{
	static const char hex[40] = "0123456789ABCDEF";
	char txt[5] = { 0, 0, 0, 0, 0 };
	_state = QUICK_SESSION_STATE_SYNWAIT;
	this->origin = origin;
	this->remote = remote;
	std::string token = "";
	for (int i = 0; i < 8; i++) {
		IUINT32 n = rand() % 0x10000;
		txt[0] = hex[n >> 12];
		txt[1] = hex[(n >> 8) & 0xf];
		txt[2] = hex[(n >> 4) & 0xf];
		txt[3] = hex[n & 0xf];
		token.append(txt);
	}
	this->token = token;
	return true;
}


//---------------------------------------------------------------------
// 发送消息：需要带上 protocol
//---------------------------------------------------------------------
bool Session::Send(ProtocolPacket *packet, int limit)
{
	bool hr = ProtocolSend(packet->protocol, packet->data(), packet->size(), limit);
	delete packet;
	return hr;
}


//---------------------------------------------------------------------
// 发送数据：
//---------------------------------------------------------------------
bool Session::Send(int protocol, const void *data, int size, int limit)
{
	return ProtocolSend(protocol, data, size, limit);
}


//---------------------------------------------------------------------
// 接收消息
//---------------------------------------------------------------------
ProtocolPacket* Session::Recv(bool peek)
{
	if (_events.empty()) {
		return NULL;
	}

	ProtocolPacket *packet = *_events.begin();

	if (peek == false) {
		_events.pop_front();
	}

	packet->hid = hid();
	packet->conv = conv();
	packet->wparam = hid();
	packet->lparam = packet->protocol;

	return packet;
}


// 接收消息 直接接收
// 成功返回消息长度，没有消息则返回-1，长度错误返回 -2
// data 为 null则返回消息长度
int Session::Recv(int *protocol, void *data, int size)
{
	if (_events.empty()) return -1;
	ProtocolPacket *packet = *_events.begin();

	if (packet->size() > size) return -2;
	if (data == NULL) return packet->size();

	size = packet->size();

	if (data) {
		packet->pop_tail(data, size);
	}

	if (protocol) protocol[0] = packet->protocol;
	_events.pop_front();
	delete packet;

	return size;
}


bool Session::IsConnecting() const
{
	return _state == QUICK_SESSION_STATE_SYN1 || _state ==  QUICK_SESSION_STATE_SYN2;
}

//---------------------------------------------------------------------
// 更新状态
//---------------------------------------------------------------------
void Session::Update(IUINT32 current, bool flush)
{
	_current = current;

	if (_updated == false) {
		_tscreate = current;
		_tsactive = current;
		_updated = true;
	}

	switch (_state) {

	// 关闭状态
	case QUICK_SESSION_STATE_CLOSED:
		break;

	// 客户端连接1：持续向服务端发送 SYN1直到收到 ACK1就转移到 STATE_SYN2
	case QUICK_SESSION_STATE_SYN1:
		if (_timeout.check(_current)) {		// 如果达到时间则发送 SYN1
			SendCommand(QUICKNET_CMD_SYN1);
		}
		break;

	// 客户端连接2：持续向服务端发送 SYN2直到收到 ACK2就转移到 STATE_ESTAB
	case QUICK_SESSION_STATE_SYN2:
		if (_timeout.check(_current)) {
			char features[4];
			iencode32u_lsb(features, _feature_local);
			SendCommand(QUICKNET_CMD_SYN2, features, 4);
		}
		break;

	// 服务端连接：等待客户端的 SYN2，返回 ACK2并切换到 STATE_ESTAB
	case QUICK_SESSION_STATE_SYNWAIT:
		break;

	// 连接建立：处理数据内容，如果还收到 SYN2，则返回 ACK2，收到 FIN后切换回 STATE_CLOSED
	case QUICK_SESSION_STATE_ESTAB:
		if (_ping) {	// 如果在 PING状态
			if (_timeout.check(_current)) {
				ProtocolPacket *packet = new ProtocolPacket(8);
				packet->protocol = 0;
				packet->cmd = QUICKNET_CMD_PING;
				packet->push_tail_uint32(_current);
				packet->push_tail_uint32(_feature_local);
				PacketOutput(packet);
			}
		}
		break;

	// 主动断开方，持续发送 FIN，等待 FACK后，切换到 STATE_CLOSED
	case QUICK_SESSION_STATE_FINWAIT:
		if (_timeout.check(_current)) {
			SendCommand(QUICKNET_CMD_FIN);
		}
		if (_shutdown.check(_current)) {
			_state = QUICK_SESSION_STATE_CLOSED;
			OnDisconnected();
		}
		break;
	}

	// 更新协议
	ProtocolUpdate(flush);
}


//---------------------------------------------------------------------
// 输入并处理第二层协议（已经解出 hid, conv的数据包）
//---------------------------------------------------------------------
void Session::PacketInput(ProtocolPacket *packet)
{
	int cmd = packet->cmd;

	// 如果更新过（设置过 _current时间）那么激活一下
	if (_updated) {
		_tsactive = _current;
	}
	else {
		delete packet;
		return;
	}

	if (packet->hid != _hid || packet->conv != _conv) {
		if (_state != QUICK_SESSION_STATE_SYN1 && 
			_state != QUICK_SESSION_STATE_SYNWAIT) {
			delete packet;
			return;
		}
	}

	switch (cmd) {

	// 不处理，上层已经处理过了	
	case QUICKNET_CMD_SYN1:
		if (_state == QUICK_SESSION_STATE_SYNWAIT) {
			SendCommand(QUICKNET_CMD_ACK1);
		}
		break;

	// 收到 SYN2
	case QUICKNET_CMD_SYN2:
		if (_state == QUICK_SESSION_STATE_SYNWAIT) {
			_state = QUICK_SESSION_STATE_ESTAB;
			SendCommand(QUICKNET_CMD_ACK2, token.c_str(), token.size());
			if (packet->size() == 4) {
				_feature_remote = packet->pop_tail_uint32();
			}
			OnConnected();
		}
		else if (_state == QUICK_SESSION_STATE_ESTAB) {
			SendCommand(QUICKNET_CMD_ACK2, token.c_str(), token.size());
		}
		break;

	// 收到 ACK1
	case QUICKNET_CMD_ACK1:
		if (_state == QUICK_SESSION_STATE_SYN1) {
			_state = QUICK_SESSION_STATE_SYN2;
			_hid = packet->hid;
			_timeout.start(_current, SESSION_CONNECT_INTERVAL);
		}
		break;

	// 收到 ACK2，连接建立
	case QUICKNET_CMD_ACK2:
		if (_state == QUICK_SESSION_STATE_SYN2) {
			_state = QUICK_SESSION_STATE_ESTAB;
			_timeout.stop();
			_ping = false;
			token.assign(packet->data(), packet->size());
			OnConnected();
		}
		break;
	
	// 收到 PING则反馈 PACK
	case QUICKNET_CMD_PING:
		packet->cmd = QUICKNET_CMD_PACK;	// 改一下命令，直接返回
		if (packet->size() == 8) {
			_feature_remote = packet->pop_tail_uint32();
		}
		PacketOutput(packet);				// 加入发包队列
		packet = NULL;						// 质空（避免后面删除）
		break;

	// 收到 PACK了
	case QUICKNET_CMD_PACK: 
		InputCommandPack(packet);
		packet = NULL;
		break;

	// 收到上层数据
	case QUICKNET_CMD_DATA:
		if (_state == QUICK_SESSION_STATE_ESTAB) {
			InputCommandData(packet);
			packet = NULL;
		}
		break;

	// 收到结束标志
	case QUICKNET_CMD_FIN: 
		if (_state != QUICK_SESSION_STATE_CLOSED) {
			_state = QUICK_SESSION_STATE_CLOSED;
			deadcode = 0;
			OnDisconnected();
			SendCommand(QUICKNET_CMD_FACK);
		}
		else {
			SendCommand(QUICKNET_CMD_FACK);
		}
		break;

	// 收到连接断开确认
	case QUICKNET_CMD_FACK:
		if (_state != QUICK_SESSION_STATE_CLOSED) {
			_state = QUICK_SESSION_STATE_CLOSED;
			OnDisconnected();
		}
		break;
	
	// 改变IP来源：IP TUNNEL支持
	case QUICKNET_CMD_CHGIP:
		if (packet->size() == (int)token.size()) {
			if (memcmp(packet->data(), token.c_str(), token.size()) == 0) {
				SendCommand(QUICKNET_CMD_CHACK, packet->data(), packet->size());
			}
		}
		break;

	// 改变IP确认：IP TUNNEL支持
	case QUICKNET_CMD_CHACK:
		break;
	}

	// 如果没有制空（没有传递到下层协议）则本层协议删除之
	if (packet) {
		delete packet;
	}
}

// 开始 PING
void Session::RemotePing()
{
	if (_state == QUICK_SESSION_STATE_ESTAB && _updated) {
		_ping = true;
		_timeout.start(_current, SESSION_PING_INTERVAL, 120, true);
	}
}

void Session::OnConnected() 
{
	if (_connect == false) {
		_connect = true;
	}
}

void Session::OnDisconnected()
{
	if (_connect) {
		_connect = false;
	}
}

void Session::InputCommandPack(ProtocolPacket *packet)
{
	IUINT32 ts = packet->pop_tail_uint32();
	delete packet;
	IINT32 rtt = itimediff(_current, ts);
	_rto.update((int)rtt);
	_ping = false;
}

void Session::InputCommandData(ProtocolPacket *packet)
{
	if (packet->protocol != QUICKNET_PROTOCOL_FEC) 
    {
        ProtocolInput(packet);
    }
    else if (_transmission) 
    {
        _transmission->PacketInput(packet);
    }
    else
    {
        delete packet;
    }
}


// 自动判断是否有传输层 Transmission，否则直接调用 PacketOutput
void Session::Transport(ProtocolPacket *packet)
{
	if (_transmission == NULL) {
		PacketOutput(packet);
	}
	else {
		// 压入协议数据：下层 FEC不知道什么协议
		packet->push_head_uint8(packet->protocol);
#ifndef NOCHECKSUM
		IUINT32 checksum = icrypt_checksum(packet->data(), packet->size());
		packet->push_head_uint8(checksum & 0xff);
#endif
		_transmission->Send(packet);
	}
}

// 传输层：接收上一层数据
void Session::TransmissionDeliver(ProtocolPacket *packet, void *user)
{
	Session *self = (Session*)user;
#ifndef NOCHECKSUM
	// 弹出 checksum
	IUINT32 checksum1 = packet->pop_head_uint8();
	IUINT32 checksum2 = icrypt_checksum(packet->data(), packet->size()) & 0xff;
	if (checksum1 != checksum2) {
		printf("ERROR FEC CHECKSUM-----------------------> %d %d\n", (int)checksum1, (int)checksum2);
		fprintf(stderr, "ERROR FEC CHECKSUM-----------------------> %d %d\n", (int)checksum1, (int)checksum2);
		fflush(stderr);
	#if 0
		throw new NetError("ERROR FEC CHECKSUM");
		abort();
	#endif
		delete packet;
		return;
	}
#endif
	// 弹出协议编号：下层 FEC不知道什么协议
	packet->protocol = packet->pop_head_uint8();
	self->ProtocolInput(packet);
}

// 传输层：发送下一层数据
void Session::TransmissionOutput(ProtocolPacket *packet, void *user)
{
	Session *self = (Session*)user;
	packet->protocol = QUICKNET_PROTOCOL_FEC;
	packet->cmd = QUICKNET_CMD_DATA;
	self->PacketOutput(packet);
}


// 协议初始化
void Session::ProtocolInit()
{
	ProtocolDestroy();
	_transmission = NULL;
	_tcp = itcp_create(0x11223344, this);
	_kcp = ikcp_create(0x11223344, this);
	_repeat = new RequestRepeat(this);
	_tcp->output = TcpOutput;
	_kcp->output = KcpOutput;
	_repeat->output = NackOutput;
	_tcp->state = ITCP_ESTAB;
	itcp_setmtu(_tcp, 1300);
	ikcp_setmtu(_kcp, 1300);
	_combinator = new Combinator(this);
	_combinator->PacketOutput = this->CombinatorOutput;
	_combinator->PacketDeliver = this->CombinatorDeliver;
}

// 协议销毁
void Session::ProtocolDestroy()
{
	if (_transmission) 
        delete _transmission;
	_transmission = NULL;
	if (_tcp) itcp_release(_tcp);
	if (_kcp) ikcp_release(_kcp);
	if (_repeat) delete _repeat;
	_repeat = NULL;
	_tcp = NULL;
	_kcp = NULL;
	if (_combinator) delete _combinator;
	_combinator = NULL;
}

// 协议发送数据
bool Session::ProtocolSend(int protocol, const void *data, int size, int limit)
{
	bool retval = true;

    if (deadmark == true)
    {
        return false;
    }

	if (_updated == false) {
		return false;
	}

	switch (protocol)
	{
	case QUICKNET_PROTOCOL_RAW: {
			ProtocolPacket *packet = new ProtocolPacket(size);
			packet->protocol = QUICKNET_PROTOCOL_RAW;
			packet->cmd = QUICKNET_CMD_DATA;
			packet->push_tail(data, size);
			Transport(packet);
		}
		break;

	case QUICKNET_PROTOCOL_KCP:
		retval = false;
		if (limit <= 0 || ikcp_waitsnd(_kcp) < limit) {
			int hr = ikcp_send(_kcp, (const char*)data, size);
			if (_nodelay && _updated) {
				_kcp->current = _current;
				//ikcp_update(_kcp, _current);
				ikcp_flush(_kcp);
			}
			if (hr >= 0) {
				retval = true;
			}	else {
				printf("kcp failed %d\n", hr);
			}
		}
		break;

	case QUICKNET_PROTOCOL_TCP: 
		retval = false;
		if (itcp_canwrite(_tcp) >= size + 4) {
			if (limit <= 0 || _tcp->slen < limit * _tcp->mss) {
				char head[4];
				iencode32u_lsb(head, size + 4);
				itcp_send(_tcp, head, 4);
				itcp_send(_tcp, (const char*)data, size);
				if (_nodelay && _updated) {
					itcp_update(_tcp, _current);
				}
				retval = true;
			}
		}
		break;

	case QUICKNET_PROTOCOL_NACK:
		if (_repeat){
			ProtocolPacket *packet = new ProtocolPacket(size);
			packet->protocol = QUICKNET_PROTOCOL_NACK;
			packet->cmd = QUICKNET_CMD_DATA;
			packet->push_tail(data, size);
			_repeat->send(packet);
			if (_nodelay && _updated) {
				_repeat->update(_current, _rto.rto());
			}
			retval = true;
		}
		break;
	}
	return retval;
}

// 协议更新
void Session::ProtocolUpdate(bool flush)
{
	if (flush == false) {
		_kcp->current = _current;
		_tcp->current = _current;
		return;
	}

	itcp_update(_tcp, _current);
	ikcp_update(_kcp, _current);

	if(_repeat) _repeat->update(_current, _rto.rto());

	while (1) 
    {
        //if (_kcp->state == -1) //dead_link;
        //{
        //    deadmark = true;
        //    deadcode = QNET_CODE_RESEND_FAIL;//
        //    break;
        //}
		int hr = ikcp_peeksize(_kcp);
		if (hr < 0) break;
		ProtocolPacket *packet = new ProtocolPacket(hr);
		packet->push_tail(NULL, hr);
		ikcp_recv(_kcp, packet->data(), hr);
		packet->protocol = QUICKNET_PROTOCOL_KCP;
		_events.push_back(packet);
	}

	while (1) {
		char head[4];
		IUINT32 size;
		int hr = itcp_peek(_tcp, head, 4);
		if (hr < 4) break;
		idecode32u_lsb(head, &size);
		if (itcp_dsize(_tcp) < (int)size) break;
		itcp_recv(_tcp, head, 4);
		hr = (int)size - 4;
		ProtocolPacket *packet = new ProtocolPacket(hr);
		packet->push_tail(NULL, hr);
		itcp_recv(_tcp, packet->data(), hr);
		packet->protocol = QUICKNET_PROTOCOL_TCP;
		_events.push_back(packet);
	}

	while (1){
		ProtocolPacket *packet = _repeat->recv_packet();
		if (packet) {
			packet->protocol = QUICKNET_PROTOCOL_NACK;
			_events.push_back(packet);
		}
		else {
			break;
		}
	}

	if (_combinator) {
		_combinator->Update(_current, false);
	}

	if (_transmission) {
		_transmission->Update(_current);
	}
}


// 协议输入
void Session::ProtocolInput(ProtocolPacket *packet)
{
	switch (packet->protocol)
	{
	case QUICKNET_PROTOCOL_RAW:
		_events.push_back(packet);
		packet = NULL;
		break;

	case QUICKNET_PROTOCOL_KCP:
		ikcp_input(_kcp, packet->data(), packet->size());
		break;

	case QUICKNET_PROTOCOL_TCP:
		itcp_input(_tcp, packet->data(), packet->size());
		break;

	case QUICKNET_PROTOCOL_NACK:
		if (_repeat) {
			_repeat->input(packet);
			packet = NULL;
		}
		break;

	case QUICKNET_PROTOCOL_COMBINE:
		CombineInput(packet);
		packet = NULL;
		break;
	}

	if (packet) {
		delete packet;
	}
}


// TCP输出
int Session::TcpOutput(const char *buf, int len, struct ITCPCB *tcp, void *user)
{
	Session *self = (Session*)user;
	ProtocolPacket *packet = new ProtocolPacket(len);
	packet->protocol = QUICKNET_PROTOCOL_TCP;
	packet->cmd = QUICKNET_CMD_DATA;
	packet->push_tail(buf, len);
	self->Transport(packet);
	return 0;
}


// KCP输出
int Session::KcpOutput(const char *buf, int len, struct IKCPCB *kcp, void *user)
{
	Session *self = (Session*)user;
	ProtocolPacket *packet = new ProtocolPacket(len);
	packet->protocol = QUICKNET_PROTOCOL_KCP;
	packet->cmd = QUICKNET_CMD_DATA;
	packet->push_tail(buf, len);
#if 0
	self->Transport(packet);
#else
	self->CombineTransport(packet);
#endif
	self->_nout_kcp++;
	return 0;
}


// RequestRepeat输出
int Session::NackOutput(ProtocolPacket *packet, void *user)
{
	/*static int test_counter = 0;
	static int test_drop = 0;
	test_counter++;
	if (test_drop && test_counter < 25)
	{
		return 0;
	}
	if (test_drop) {
		test_drop = 0;
		test_counter = 0;
	}
	if (test_counter >= 100)
	{
		test_drop = 1;
		test_counter = 0;
		return 0;
	}*/

	Session *self = (Session*)user;
	//packet->protocol = QUICKNET_PROTOCOL_NACK;
	//packet->cmd = QUICKNET_CMD_DATA;
#if 0
	self->Transport(packet);
#else
	self->CombineTransport(packet);
#endif
	return 0;
}

// 合并数据包传输，判断是否开启合并，开启的话就使用合并，下层调用 Transport
void Session::CombineTransport(ProtocolPacket *packet)
{
	if ((_feature_local & QUICKNET_FEATURE_COMBINE_SEND) && 
		(_feature_remote & QUICKNET_FEATURE_COMBINE_RECV)) {
		_combinator->Send(packet);
	}
	else {
		Transport(packet);
	}
}

// 接收到 Combinator 数据
void Session::CombineInput(ProtocolPacket *packet)
{
	_combinator->Input(packet);
}

// 合并层：传递上一层数据
void Session::CombinatorDeliver(const void *data, int size, int protocol, void *user)
{
	Session *self = (Session*)user;
	ProtocolPacket *packet = NULL;

	switch (protocol) {
	case QUICKNET_PROTOCOL_RAW:
		packet = new ProtocolPacket(size);
		packet->push_tail(data, size);
		packet->protocol = QUICKNET_PROTOCOL_RAW;
		self->_events.push_back(packet);
		break;

	case QUICKNET_PROTOCOL_KCP:
		ikcp_input(self->_kcp, (const char*)data, size);
		break;

	case QUICKNET_PROTOCOL_TCP:
		itcp_input(self->_tcp, (const char*)data, size);
		break;

	case QUICKNET_PROTOCOL_NACK:
		if (self->_repeat) {
			packet = new ProtocolPacket(size);
			packet->push_tail(data, size);
			packet->protocol = QUICKNET_PROTOCOL_NACK;
			self->_repeat->input(packet);
		}
		break;
	}
}

// 合并层：发送下一层数据
void Session::CombinatorOutput(ProtocolPacket *packet, void *user)
{
	Session *self = (Session*)user;
	self->Transport(packet);
}

// 设置 MTU
void Session::SetMTU(int mtu)
{
	if (mtu > 32 && mtu <= 0x10000) {
		_mtu = mtu;
		_mss = mtu - QUICKNET_SESSION_OVERHEAD;
		if (_transmission) {
			_mss -= _transmission->GetOverhead();
		}
		itcp_setmtu(_tcp, _mss);
		ikcp_setmtu(_kcp, _mss);
	}
}

int Session::GetOption(int option) const
{
    int hr = 0;
    switch (option) {
    case QUICKNET_OPT_NODELAY:
        hr = _nodelay;
        break;
    case QUICKNET_OPT_KCP_INTERVAL:
        hr = (int)_kcp->interval;
        break;
    case QUICKNET_OPT_KCP_NODELAY:
		hr = (int)_kcp->nodelay;
        break;
    case QUICKNET_OPT_KCP_RESEND:
		hr = (int)_kcp->fastresend;
        break;
    case QUICKNET_OPT_KCP_NC:
		hr = (int)_kcp->nocwnd;
        break;
    case QUICKNET_OPT_KCP_SNDWND:
		hr = (int)_kcp->snd_wnd;
        break;
    case QUICKNET_OPT_KCP_RCVWND:
		hr = (int)_kcp->rcv_wnd;
        break;
    case QUICKNET_OPT_TCP_NODELAY:
		hr = (int)_tcp->nodelay;
        break;
    case QUICKNET_OPT_TCP_BUFSIZE:
		hr = (int)_tcp->buf_size;
        break;
	case QUICKNET_OPT_NACK_BUFSIZE:
		hr = _repeat->get_pull_size();
		break;
	case QUICKNET_OPT_NACK_SKIPSIZE:
		hr = _repeat->get_skip_size();
		break;
    case QUICKNET_OPT_KCP_XMIT:
        hr = (int)_kcp->xmit;
        break;
    case QUICKNET_OPT_KCP_DEAD_LINK:
        hr = (int)_kcp->dead_link;
        break;
	case QUICKNET_OPT_KCP_RTO:
		hr = (int)_kcp->rx_rto;
		break;
	case QUICKNET_OPT_KCP_WAITSND:
		hr = ikcp_waitsnd(_kcp);
		break;
	case QUICKNET_OPT_KCP_SNDPKT:
		hr = _nout_kcp;
		break;
	case QUICKNET_OPT_KCP_RTT:
		hr = (int)_kcp->rx_rttval;
		break;
    case QUICKNET_OPT_KCP_OUTWND:
		hr = (int)_kcp->nsnd_que;
		break;
	case QUICKNET_OPT_KCP_WNDSND:
		hr = (int)_kcp->nsnd_buf;
		break;
	case QUICKNET_OPT_KCP_STREAM:
		hr = (int)_kcp->stream;
		break;
	case QUICKNET_OPT_FEATURE_LOC:
		hr = _feature_local;
		break;
	case QUICKNET_OPT_FEATURE_RMT:
		hr = _feature_remote;
		break;
	case QUICKNET_OPT_COMBINE_LIMIT:
		hr = _combinator->Option(2, 0);
		break;
	case QUICKNET_OPT_COMBINE_PERIOD:
		hr = _combinator->Option(3, 0);
		break;
	default:
        {
        hr = -1;
        if (_transmission) 
        {
            hr = _transmission->GetStatus(option);
        }
        break;
        }
    }
    return hr;
}
// 协议更新
int Session::Option(int option, int value)
{
	int hr = 0;
	switch (option) {
	case QUICKNET_OPT_NODELAY:
		_nodelay = value;
		break;
	case QUICKNET_OPT_KCP_INTERVAL:
		_kcp->interval = value;
		break;
    case QUICKNET_OPT_KCP_DEAD_LINK:
        if (value >= 5)
        {
            _kcp->dead_link = value;
        }
        break;
	case QUICKNET_OPT_KCP_NODELAY:
		ikcp_nodelay(_kcp, value, -1, -1, -1);
		break;
	case QUICKNET_OPT_KCP_RESEND:
		ikcp_nodelay(_kcp, -1, -1, value, -1);
		break;
	case QUICKNET_OPT_KCP_NC:
		ikcp_nodelay(_kcp, -1, -1, -1, value);
		break;
	case QUICKNET_OPT_KCP_SNDWND:
		ikcp_wndsize(_kcp, value, -1);
		break;
	case QUICKNET_OPT_KCP_RCVWND:
		ikcp_wndsize(_kcp, -1, value);
		break;
	case QUICKNET_OPT_KCP_MINRTO:
		_kcp->rx_minrto = value;
		break;
	case QUICKNET_OPT_TCP_NODELAY:
		itcp_option(_tcp, value, 0);
		break;
	case QUICKNET_OPT_TCP_BUFSIZE:
		itcp_setbuf(_tcp, value);
		break;
	case QUICKNET_OPT_NACK_BUFSIZE:
		_repeat->set_pull_size(value);
		break;
	case QUICKNET_OPT_NACK_SKIPSIZE:
		_repeat->set_skip_size(value);
		break;
	case QUICKNET_OPT_KCP_LOG:
		if (value <= 0) {
			_kcp->writelog = NULL;
		}
		else {
			_kcp->writelog = KcpLog;
			_kcp->logmask = value;
		}
		break;
	case QUICKNET_OPT_KCP_STREAM:
		_kcp->stream = (value <= 0)? 0 : 1;
		break;
	case QUICKNET_OPT_NACK_LOG:
		if (value <= 0) {
			_repeat->writelog = NULL;
		} else {
			_repeat->writelog = NackLog;
			_repeat->logmask = value;
		}
		break;
	case QUICKNET_OPT_FEATURE_LOC:
		_feature_local = ((IUINT32)value) & QUICKNET_FEATURES;
		break;
	case QUICKNET_OPT_FEATURE_RMT:
		hr = -1;
		break;
	case QUICKNET_OPT_COMBINE_LIMIT:
		_combinator->Option(0, value);
		break;
	case QUICKNET_OPT_COMBINE_PERIOD:
		_combinator->Option(1, value);
		break;
	default:
		hr = -1;
		if (_transmission) {
			hr = _transmission->Option(option, value);
		}
		break;
	}
	return hr;
}


// 提交所有东西
void Session::ProtocolFlush()
{
	if (_updated) {
		ikcp_update(_kcp, _current);
		itcp_update(_tcp, _current);
		if (_repeat) _repeat->update(_current, _rto.rto());
	}
	ikcp_flush(_kcp);
}


// 安装协议
bool Session::TransmissionInstall(Transmission::Factory factory)
{
	if (_transmission) {
		delete _transmission;
		_transmission = NULL;
	}

	if (factory) _transmission = factory();

	if (_transmission) {
		_transmission->user = this;
		_transmission->PacketOutput = TransmissionOutput;
		_transmission->PacketDeliver = TransmissionDeliver;
	}

	if (_updated && _transmission) {
		_transmission->Update(_current);
	}

	return true;
}

void Session::KcpLog(const char *log, struct IKCPCB *kcp, void *user)
{
	Session *self = (Session*)user;
	self->trace->out(TRACE_KCP, "[KCP] %s", log);
}

void Session::NackLog(const char *log, void *user)
{
	Session *self = (Session*)user;
	self->trace->out(TRACE_SESSION, "[NACK] hid:%d %s", self->hid(), log);
}

void Session::GetNACKStatInfo(int* packets, int *pull, int* pullpkts, int *lost, int *pulltimeout, int *skip, int *totalskippkt)
{
	if (_repeat)
		_repeat->get_stat_info(packets, pull, pullpkts, lost, pulltimeout, skip, totalskippkt);
	else
	{
		*packets = 0; 
		*pull = 0;
		*pullpkts = 0;
		*lost = 0;
		*pulltimeout = 0;
		*skip = 0;
		*totalskippkt = 0;
	}
}


// 取得 RTT
int Session::GetRtt() const
{
	return _rto.rto();
}

// 取得 KCP的待发送数据
int Session::GetPending(int what) const
{
	if (_kcp == NULL) return 0;
	return ikcp_waitsnd(_kcp);
}


NAMESPACE_END(QuickNet)



