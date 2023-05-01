#include "Combinator.h"

NAMESPACE_BEGIN(QuickNet)


//---------------------------------------------------------------------
// inline functions
//---------------------------------------------------------------------
static inline int PacketSize(const ProtocolPacket *packet) {
	return packet->size() + 2;
}


//---------------------------------------------------------------------
// Constructor
//---------------------------------------------------------------------
Combinator::Combinator(void *user)
{
	_initialized = false;
	_user = user;
	_total_size = 0;
	_limit = 900;
	_current = 0;
	_timeslap = 0;
	_period = 20;
	PacketOutput = NULL;
	PacketDeliver = NULL;
}


//---------------------------------------------------------------------
// Destructor
//---------------------------------------------------------------------
Combinator::~Combinator()
{
	ProtocolUdp::ClearPacketVector(_pending);
}


//---------------------------------------------------------------------
// Update interval
//---------------------------------------------------------------------
void Combinator::Update(IUINT32 current, bool flush)
{
	_current = current;
	if (_initialized == false) {
		_timeslap = current;
		_initialized = true;
	}
	int need = 0;
	if (flush) {
		_timeslap = _current;
	}
	while (_current >= _timeslap) {
		need++;
		_timeslap += (_period < 1)? 1 : _period;
	}
	if (flush || need) {
		Flush();
	}
}


//---------------------------------------------------------------------
// send to lower level
//---------------------------------------------------------------------
void Combinator::Send(ProtocolPacket *packet)
{
	if (_initialized == false) {
		delete packet;
	}
	else if (packet->protocol == QUICKNET_PROTOCOL_COMBINE) {
		delete packet;
	}
	else if (packet->protocol > 0xf) {
		Output(packet);
	}
	else if (packet->size() > 4096) {
		Flush();
		Output(packet);
	}
	else {
		int size = PacketSize(packet);
		if (size + _total_size > _limit) {
			Flush();
		}
		_pending.push_back(packet);
		_total_size += size;
		if (_total_size >= _limit) {
			Flush();
		}
	}
}


//---------------------------------------------------------------------
// send to lower level
//---------------------------------------------------------------------
void Combinator::Flush()
{
	int count = (int)_pending.size();
	if (count == 1) {
		ProtocolPacket *packet = _pending[0];
		Output(packet);
		_pending.resize(0);
		_total_size = 0;
	}
	else if (count > 1) {
		ProtocolPacket *packet = new ProtocolPacket(_total_size + 8);
		for (int i = 0; i < count; i++) {
			ProtocolPacket *p = _pending[i];
			int size = p->size();
			packet->push_tail_uint16((size << 4) | (p->protocol & 0xf));
			packet->push_tail(p->data(), size);
			delete p;
		}
		packet->protocol = QUICKNET_PROTOCOL_COMBINE;
		Output(packet);
		_pending.resize(0);
		_total_size = 0;
	}
}


//---------------------------------------------------------------------
// input from lower level packet
//---------------------------------------------------------------------
void Combinator::Input(ProtocolPacket *packet)
{
	if (packet->protocol != QUICKNET_PROTOCOL_COMBINE) {
		this->Deliver(packet->data(), packet->size(), packet->protocol);
	}	
	else {
		while (1) {
			if (packet->size() < 2) break;
			int head = packet->pop_head_uint16();
			int size = head >> 4;
			int protocol = head & 0xf;
			if (packet->size() < size) break;
			this->Deliver(packet->data(), size, protocol);
			packet->pop_head(NULL, size);
		}
		delete packet;
	}
}


//---------------------------------------------------------------------
// input from lower level packet
//---------------------------------------------------------------------
int Combinator::Option(int opt, int value)
{
	int hr = 0;
	switch (opt) {
	case 0:
		_limit = (value < 100)? 100 : ((value > 2048)? 2048 : value);
		break;
	case 1:
		_period = (value < 2)? 2 : ((value > 1000)? 1000 : value);
		break;
	case 2:
		hr = _limit;
		break;
	case 3:
		hr = _period;
		break;
	}
	return hr;
}



NAMESPACE_END(QuickNet)



