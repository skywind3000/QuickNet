//=====================================================================
//
// RequestRepeat.cpp - Negitive Ack ARQ implementation
//
// NOTE:
// 拉包协议实现，接收端发现序号不连续则发送 NACK到发送方。
//
//=====================================================================
#include "RequestRepeat.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>

#define NACK_LOG_DEBUG	1
#define NACK_LOG_INFO	2
#define NACK_LOG_ERROR	4

NAMESPACE_BEGIN(QuickNet)
//---------------------------------------------------------------------
// RequestRepeat
//---------------------------------------------------------------------

RequestRepeat::RequestRepeat(void* user)
{
	output = NULL;
	m_pUser = user;
	m_nSendPacketCounter = 0;
	m_nMaxPacketSn = 0;
	m_nLastRecvSn = 0;
	m_nPullSize = 160;
	m_listSendPacket.clear();
	m_listSendPacketSn.clear();
	m_listSecondPullPacketSn.clear();
	m_mapSendPacket.clear();
	m_mapRecvPacket.clear();
	m_listRecvData.clear();

	m_nTimesPull = 0;
	m_nPacketsLost = 0;
	m_nTimesRepeat = 0;
	m_nPacketsPullTimeout = 0;
	m_nTimesSkip = 0;
	m_nPacketsSkip = 0;
	m_nPacketsPull = 0;
	m_nSkipSize = QUICKNET_NACK_PKTNUM_PULL_SKIP;

	writelog = NULL;
	logmask = 0;
}

RequestRepeat::~RequestRepeat()
{
	m_nTimesPull = 0;
	m_nPacketsLost = 0;
	m_nTimesRepeat = 0;
	m_nPacketsPullTimeout = 0;
	m_nTimesSkip = 0;
	m_nPacketsSkip = 0;
	m_nPacketsPull = 0;

	output = NULL;
	m_nSendPacketCounter = 0;
	m_nPullSize = 0;
	m_nSkipSize = 0;
	m_nMaxPacketSn = 0;
	m_nLastRecvSn = 0;
	m_listRecvData.clear();
	m_listSendPacketSn.clear();
	m_listSecondPullPacketSn.clear();
	std::list<ProtocolPacket*>::iterator itl = m_listSendPacket.begin();
	for ( ; itl != m_listSendPacket.end(); itl++) {
		delete (ProtocolPacket*)*itl;
	}
	m_listSendPacket.clear();
	//std::map<IUINT32, ProtocolPacket*>::iterator it = m_mapSendPacket.begin();
	PacketHash::iterator it = m_mapSendPacket.begin();
	for ( ; it != m_mapSendPacket.end(); it++) {
		delete (ProtocolPacket*)it->second;
	}
	it = m_mapRecvPacket.begin();
	for ( ; it != m_mapRecvPacket.end(); it++) {
		delete (ProtocolPacket*)it->second;
	}
	m_mapSendPacket.clear();
	m_mapRecvPacket.clear();

	writelog = NULL;
	logmask = 0;
}

void RequestRepeat::send_flag_pull(int sn)
{
	ProtocolPacket *packet = new ProtocolPacket(0);
	packet->protocol = QUICKNET_PROTOCOL_NACK;
	packet->cmd = QUICKNET_CMD_DATA;
	packet->sn = sn;
	packet->flag = QUICKNET_FLAG_PULL; 
	packet->push_head_uint32(packet->sn);
	packet->push_head_uint8(packet->flag);
	++m_nTimesPull;
	if (output) output(packet, m_pUser);
	else delete packet;
	log(NACK_LOG_INFO, "send flag pull, sn:%d, times:%d", sn, m_nTimesPull);
}

void RequestRepeat::log(int mask, const char *fmt, ...)
{
	char buffer[1024];
	va_list argptr;
	if ((mask & logmask) == 0 || writelog == 0) return;
	va_start(argptr, fmt);
	vsprintf(buffer, fmt, argptr);
	va_end(argptr);
	writelog(buffer, m_pUser);
}

int RequestRepeat::input(ProtocolPacket *packet)
{
	IUINT32 sn;
	IUINT8 flag;
	RecvSeq seq;
	RecvSeq second_seq;

	flag = packet->pop_head_uint8();
	sn = packet->pop_head_uint32();
	if (flag == QUICKNET_FLAG_DATA) {
		// 简单拉包策略
		if (m_nMaxPacketSn + 1 < sn) {
			int count = sn - m_nMaxPacketSn -1;
			for(int i = 1; i <= count; i++)
			{
				if (count >= m_nSkipSize)
				{
					seq.m_nSendTime = 0;
					if (m_nCurrent > QUICKNET_NACK_RTO_DEFAULT_MAX_X2)
						seq.m_nSendTime = m_nCurrent - QUICKNET_NACK_RTO_DEFAULT_MAX_X2;
					seq.m_nSn = m_nMaxPacketSn + i;
					m_listRecvData.push_back(seq);
					log(NACK_LOG_INFO, "packet input skip pull, count:%d sn:%d, max_sn:%d recv_sn:%d ", count, seq.m_nSn, m_nMaxPacketSn, m_nLastRecvSn);
				}
				else
				{
					send_flag_pull(m_nMaxPacketSn + i);
					send_flag_pull(m_nMaxPacketSn + i);
					m_nPacketsPull++;
					seq.m_nSendTime = m_nCurrent;
					seq.m_nSn = m_nMaxPacketSn + i;
					m_listRecvData.push_back(seq);
					// 第二次拉包相关
					second_seq.m_nSendTime = m_nCurrent + (m_nRtt * 3 / 5);
					second_seq.m_nSn = m_nMaxPacketSn + i;
					m_listSecondPullPacketSn.push_back(second_seq);
				}
			}
			if (count >= m_nSkipSize)
			{
				m_nTimesSkip++;
				m_nPacketsSkip += count;
			}
			//m_nMaxPacketSn = sn;
		}
		if (m_nLastRecvSn < sn) {
			// 进缓存
			seq.m_nSendTime = m_nCurrent;
			seq.m_nSn = sn;
			if (sn <= m_nMaxPacketSn) {
				//std::map<IUINT32, ProtocolPacket*>::iterator it = m_mapRecvPacket.find(sn);
				PacketHash::iterator it = m_mapRecvPacket.find(sn);
				if (it == m_mapRecvPacket.end()) {
					//m_listRecvData.push_back(seq);
					m_mapRecvPacket[sn] = packet;
					log(NACK_LOG_INFO, "packet input repeat normal, sn:%d, max_sn:%d recv_sn:%d", sn, m_nMaxPacketSn, m_nLastRecvSn);
				} else {
					++m_nTimesRepeat;
					delete packet;
					log(NACK_LOG_INFO, "packet input repeat 2, sn:%d, max_sn:%d recv_sn:%d", sn, m_nMaxPacketSn, m_nLastRecvSn);
				}
			} else {
				m_listRecvData.push_back(seq);
				m_mapRecvPacket[sn] = packet;
				m_nMaxPacketSn = sn;
				log(NACK_LOG_INFO, "packet input normal, sn:%d, max_sn:%d recv_sn:%d", sn, m_nMaxPacketSn, m_nLastRecvSn);
			}
		} else {
			++m_nPacketsPullTimeout;
			delete packet;
			log(NACK_LOG_INFO, "packet input repeat timeout, sn:%d, max_sn:%d recv_sn:%d", sn, m_nMaxPacketSn, m_nLastRecvSn);
		}
	} else if (flag == QUICKNET_FLAG_PULL) {
		//std::map<IUINT32, ProtocolPacket*>::iterator it = m_mapSendPacket.find(sn);
		PacketHash::iterator it = m_mapSendPacket.find(sn);
		log(NACK_LOG_INFO, "packet input ack, sn:%d, max_sn:%d recv_sn:%d", sn, m_nMaxPacketSn, m_nLastRecvSn);
		if (it != m_mapSendPacket.end()) {
			log(NACK_LOG_INFO, "packet input ack output, sn:%d, max_sn:%d recv_sn:%d", sn, m_nMaxPacketSn, m_nLastRecvSn);
			// 只拉包一次的话，不需要重新分配了，直接在缓存删除，序号列表继续占位
			//if (output) output((ProtocolPacket*)it->second, m_pUser);
			//else delete (ProtocolPacket*)it->second;
			//m_mapSendPacket.erase(sn);
			// 2012.9.25 修改成两次拉包
			if (output)
			{
				ProtocolPacket *copy;
				copy = it->second->copy();
				output(copy, m_pUser);
			}
		}
		delete packet;
	} else {
		delete packet;
		return -1;
	}
	return 0;
}
	
int RequestRepeat::send(ProtocolPacket *packet)
{
	//m_listSendPacket.push_back(packet);

	ProtocolPacket *copy;
	m_nSendPacketCounter++;
	packet->sn = m_nSendPacketCounter;
	packet->flag = QUICKNET_FLAG_DATA;
	packet->push_head_uint32(packet->sn);
	packet->push_head_uint8(packet->flag);
	// 克隆
	copy = packet->copy();
	if (output) output(packet, m_pUser);
	else delete packet;
	// 进入拉包缓存
	if ((int)m_mapSendPacket.size() >= m_nPullSize) {
		IUINT32 sn = *m_listSendPacketSn.begin();
		m_listSendPacketSn.pop_front();
		//std::map<IUINT32, ProtocolPacket*>::iterator it = m_mapSendPacket.find(sn);
		PacketHash::iterator it = m_mapSendPacket.find(sn);
		// 已经被拉的包不需要再释放
		if (it != m_mapSendPacket.end()) {
			delete (ProtocolPacket*)it->second;
			m_mapSendPacket.erase(it);
		}
	}
	m_listSendPacketSn.push_back(m_nSendPacketCounter);
	m_mapSendPacket[m_nSendPacketCounter] = copy;
	//log(NACK_LOG_INFO, "send, sn:%d", m_nSendPacketCounter);
	return 0;
}

int RequestRepeat::update(IUINT32 current, int rto)
{
	m_nCurrent = current;
	if (rto <= 0) m_nRtt = QUICKNET_NACK_RTO_DEFAULT;
	else if (rto < QUICKNET_NACK_RTO_DEFAULT_MIN) m_nRtt = QUICKNET_NACK_RTO_DEFAULT_MIN_X2;
	else m_nRtt = rto > QUICKNET_NACK_RTO_DEFAULT_MAX ? QUICKNET_NACK_RTO_DEFAULT_MAX_X2 : rto*2;
	// 更新第二次拉包
	while (!m_listSecondPullPacketSn.empty())
	{
		RecvSeq seq;
		seq = *m_listSecondPullPacketSn.begin();
		if (itimediff(m_nCurrent, seq.m_nSendTime) >= 0) {
			m_listSecondPullPacketSn.pop_front();
			if (m_nLastRecvSn < seq.m_nSn) {
				PacketHash::iterator it = m_mapRecvPacket.find(seq.m_nSn);
				if (it == m_mapRecvPacket.end()) {
					send_flag_pull(seq.m_nSn);	
				}
			}
		} else {
			break;
		}
	}
	return 0;
}

ProtocolPacket *RequestRepeat::recv_packet()
{
	ProtocolPacket *packet;
	RecvSeq seq;

	while(m_mapRecvPacket.size() > 0)
	{
		PacketHash::iterator it;
		//std::map<IUINT32, ProtocolPacket*>::iterator it;
		it = m_mapRecvPacket.find(m_nLastRecvSn + 1);
		if (it != m_mapRecvPacket.end()) {
			packet = (ProtocolPacket *)it->second;
			m_nLastRecvSn = it->first;
			seq = *m_listRecvData.begin();
			if(seq.m_nSn != m_nLastRecvSn)
			{
				log(NACK_LOG_ERROR, "recv packet error, sn1:%d, sn2:%d", m_nLastRecvSn, seq.m_nSn);
				assert(false);
			}
			int size = m_listRecvData.size();
			m_listRecvData.pop_front();
			m_mapRecvPacket.erase(it);
			log(NACK_LOG_INFO, "recv packet exist, sn:%d  size_old:%d size_new:%d", m_nLastRecvSn, size, m_listRecvData.size());
			return packet;
		}
		if (m_listRecvData.empty())
		{
			log(NACK_LOG_ERROR, "recv packet error, list is empty, but map has items");
			assert(false);
		}
		seq = *m_listRecvData.begin();
		if (itimediff(m_nCurrent, seq.m_nSendTime) >= (IINT32)m_nRtt) {
			m_listRecvData.pop_front();
			m_nLastRecvSn = seq.m_nSn;
			++m_nPacketsLost;
			log(NACK_LOG_INFO, "recv packet lost, sn:%d, lost:%d rtt:%d", m_nLastRecvSn, m_nPacketsLost, m_nRtt);
		} else {
			break;
		}
	}
	return NULL;
}

int RequestRepeat::set_pull_size(int size)
{
	m_nPullSize = size;
	return 0;
}

int RequestRepeat::get_pull_size()
{
	return m_nPullSize;
}

int RequestRepeat::get_skip_size()
{
	return m_nSkipSize;
}

void RequestRepeat::set_skip_size(int size)
{
	m_nSkipSize = size;
}


void RequestRepeat::get_stat_info(int* packets, int *pull, int* pullpkts, int *lost, int *pulltimeout, int *skip, int *totalskippkt)
{
	*packets = m_nLastRecvSn;
	*pull = m_nPacketsPull;
	*pullpkts = m_nTimesPull;
	*lost = m_nPacketsLost;
	*pulltimeout = m_nPacketsPullTimeout;
	*skip = m_nTimesSkip;
	*totalskippkt = m_nPacketsSkip;
}

NAMESPACE_END(QuickNet)

