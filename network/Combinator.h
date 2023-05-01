#ifndef __COMBINATOR_H__
#define __COMBINATOR_H__

#include "ProtocolBasic.h"

NAMESPACE_BEGIN(QuickNet)

//---------------------------------------------------------------------
// Combinator
//---------------------------------------------------------------------
class Combinator
{
public:
	Combinator(void *user);
	virtual ~Combinator();

public:

	// update interval
	void Update(IUINT32 current, bool force);

	// send upper level
	void Send(ProtocolPacket *packet);

	// input from lower level packet
	void Input(ProtocolPacket *packet);

	// flush data
	void Flush();

	// Option: 0(limit), 1(period)
	int Option(int opt, int value);

public:
	// 输出下层协议的 packet，函数指针，外面提供
	void (*PacketOutput)(ProtocolPacket *packet, void *user);

	// 输出上层协议的 packet，函数指针，外面提供
	void (*PacketDeliver)(const void *data, int size, int protocol, void *user);

	// 输出下层数据包
	inline void Output(ProtocolPacket *packet) { 
		if (PacketOutput) PacketOutput(packet, _user);
		else delete packet;
	}

	// 输出上层数据包
	inline void Deliver(const void *data, int size, int protocol) {
		if (PacketDeliver) {
			PacketDeliver(data, size, protocol, _user);
		}
	}

protected:
	void *_user;
	PacketVector _pending;
	bool _initialized;
	int _limit;
	int _total_size;
	IUINT32 _current;
	IUINT32 _timeslap;
	IUINT32 _period;
};


NAMESPACE_END(QuickNet)

#endif



