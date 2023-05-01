//=====================================================================
// 
// TransportUdp.cpp - UDP 传输以及 Qos设置
//
// NOTE:
// for more information please see the readme file.
//
//=====================================================================
#include "TransportUdp.h"

NAMESPACE_BEGIN(QuickNet)

const IUINT32 QUICKNET_UDP_OVERHEAD	 = 42;

//---------------------------------------------------------------------
// TransportUdp
//---------------------------------------------------------------------
TransportUdp::TransportUdp()
{
	_sock = -1;
	_port = -1;
	_ip = 0;
}

TransportUdp::~TransportUdp()
{
	close();
}

bool TransportUdp::open(int port, IUINT32 ip, bool block)
{
	sockaddr local;
	close();
	memset(&local, 0, sizeof(local));
	isockaddr_set(&local, ip, port);
	_sock = isocket_udp_open(&local, 0, block? 512 : 0);
	if (_sock < 0) {
		return false;
	}
	//ienable(_sock, ISOCK_REUSEADDR);
	isockname(_sock, _local_address.address(), NULL);
	_ip = ip;
	_port = port;
	set_tos(46, -1);  // EF
	return true;
}

void TransportUdp::close()
{
	memset(&_stat_current, 0, sizeof(_stat_current));

	if (_sock >= 0) {
		iclose(_sock);
		_sock = -1;
	}

	_ip = 0;
	_port = -1;
	_local_address.set((unsigned long)0, 0);
}

int TransportUdp::set_tos(int dscp, int cos)
{
	if (_sock < 0) return -1;
#ifdef __APPLE__
	return -1;
#elif defined(__unix) && defined(SO_PRIORITY)
	if (dscp < 0 || dscp > 63) return -2;
	if (cos < -1 || cos > 7) return -3;
	int tos = dscp << 2;
	if(0 != isetsockopt(_sock, IPPROTO_IP, IP_TOS, (char*)&tos, 4 )) {
		return -4;
	}
	if (cos != -1) {
		if (0 != isetsockopt(_sock, SOL_SOCKET, SO_PRIORITY, (char*) &cos, 4)) {
			return -5;
		}
	}
	return 0;
#else
	return -1;
#endif
}

int TransportUdp::send(const void *data, int len, const sockaddr *addr)
{
	if (_sock < 0) return -3;
	int hr = isendto(_sock, data, len, 0, addr, 0);
	if (hr < 0) {
		_stat_current.discard_count++;
		_stat_current.discard_size += len;
		_stat_current.discard_data += len + QUICKNET_UDP_OVERHEAD;
		if (ierrno() == IEAGAIN) {
			return -1;
		}
		return -2;
	}
	_stat_current.out_count++;
	_stat_current.out_size += len;
	_stat_current.out_data += len + QUICKNET_UDP_OVERHEAD;
	return hr;
}

int TransportUdp::send(const void *data, int len, IUINT32 ip, int port)
{
	System::SockAddress remote(ip, port);
	return send(data, len, remote.address());
}

int TransportUdp::send(const void *data, int len, const System::SockAddress &addr)
{
	return send(data, len, addr.address());
}

int TransportUdp::send(const void *data, int len, const char *ip, int port)
{
    System::SockAddress remote(ip, port);
  	return send(data, len, remote.address());
}

int TransportUdp::recv(void *data, int len, sockaddr *addr)
{
	if (_sock < 0) return -3;
	int hr = irecvfrom(_sock, data, len, 0, addr, 0);
	if (hr < 0) {
		if (ierrno() == IEAGAIN) return -1;
		return -2;
	}
	_stat_current.in_count++;
	_stat_current.in_size += hr;
	_stat_current.in_data += hr + QUICKNET_UDP_OVERHEAD;
	return hr;
}

int TransportUdp::recv(void *data, int len, IUINT32 *ip, int *port)
{
	System::SockAddress remote;
	int hr = recv(data, len, remote.address());
	ip[0] = remote.get_ip();
	port[0] = remote.get_port();
	return hr;
}

int TransportUdp::recv(void *data, int len, System::SockAddress &addr)
{
	return recv(data, len, addr.address());
}

int TransportUdp::recv(void *data, int len, char *ip, int *port)
{
	System::SockAddress remote;
	int hr = recv(data, len, remote.address());
	remote.get_ip_text(ip);
	port[0] = remote.get_port();
	return hr;
}

int TransportUdp::set_buffer(int sndbuf, int rcvbuf)
{
	return isocket_set_buffer(_sock, rcvbuf, sndbuf);
}

int TransportUdp::get_buffer(int *sndbuf, int *rcvbuf)
{
	if (_sock < 0) return -1;
	if (sndbuf) {
		long bufsize = 0;
		int len = sizeof(long);
		int hr = igetsockopt(_sock, SOL_SOCKET, SO_SNDBUF, (char*)&bufsize, &len);
		if (hr < 0) return -2;
		*sndbuf = (int)bufsize;
	}
	if (rcvbuf) {
		long bufsize = 0;
		int len = sizeof(long);
		int hr = igetsockopt(_sock, SOL_SOCKET, SO_RCVBUF, (char*)&bufsize, &len);
		if (hr < 0) return -3;
		*rcvbuf = (int)bufsize;
	}
	return 0;
}

int TransportUdp::poll(int mask, IUINT32 millisec)
{
	if (_sock < 0) return 0;
	return ipollfd(_sock, mask, millisec);
}

void TransportUdp::stat(statistic &st) 
{
	st = _stat_current;
}

void TransportUdp::local(System::SockAddress &address)
{
	address = _local_address;
}

NAMESPACE_END(QuickNet)



