//=====================================================================
// 
// TransportUdp.h - UDP 传输以及 Qos设置
//
// NOTE:
// for more information please see the readme file.
//
//=====================================================================
#ifndef __TRANSPORT_UDP_H__
#define __TRANSPORT_UDP_H__

#include "../system/system.h"

NAMESPACE_BEGIN(QuickNet)
//---------------------------------------------------------------------
// TransportUdp
//---------------------------------------------------------------------
class TransportUdp
{
public:
	TransportUdp();
	virtual ~TransportUdp();

	// 打开并绑定端口
	bool open(int port, IUINT32 ip = 0, bool block = false);
	void close();

	int set_tos(int dscp, int cos);

	int send(const void *data, int len, const sockaddr *addr);
	int send(const void *data, int len, IUINT32 ip, int port);
	int send(const void *data, int len, const System::SockAddress &addr);
	int send(const void *data, int len, const char *ip, int port);
	
	int recv(void *data, int len, sockaddr *addr);
	int recv(void *data, int len, IUINT32 *ip, int *port);
	int recv(void *data, int len, System::SockAddress &addr);
	int recv(void *data, int len, char *ip, int *port);

	int set_buffer(int sndbuf = -1, int rcvbuf = -1);
	int get_buffer(int *sndbuf = NULL, int *rcvbuf = NULL);

	int poll(int mask, IUINT32 millisec);

	void local(System::SockAddress &address);

public:
	struct statistic {
		IUINT64 out_count;
		IUINT64 out_size;
		IUINT64 out_data;
		IUINT64 in_count;
		IUINT64 in_size;
		IUINT64 in_data;
		IUINT64 discard_count;
		IUINT64 discard_size;
		IUINT64 discard_data;
	};

	void stat(statistic &st);

protected:
	int _sock;
	int _port;
	IUINT32 _ip;
	System::SockAddress _local_address;
	statistic _stat_current;
};


NAMESPACE_END(QuickNet)

#endif


