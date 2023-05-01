//=====================================================================
//
// RequestRepeat.h - Negitive Ack ARQ implementation
//
// NOTE:
// 拉包协议实现，接收端发现序号不连续则发送 NACK到发送方。
//
//=====================================================================
#ifndef __REQUEST_REPEAT_H__
#define __REQUEST_REPEAT_H__

#include "PacketBuffer.h"
#include "ProtocolBasic.h"

#include<vector>
#include<map>

#ifdef __GNUC__
	#ifdef __DEPRECATED
		#undef __DEPRECATED
	#endif
	#include <ext/hash_map>
	namespace stdext { using namespace __gnu_cxx; }
	namespace __gnu_cxx {
		template<> struct hash< std::string > {
			size_t operator()( const std::string& x ) const {
				return hash< const char* >()( x.c_str() );
			}
		};
	}
#else
	#ifndef _MSC_VER
		#include <hash_map>
	#elif (_MSC_VER < 1300)
		#include <map>
		#define IHAVE_NOT_HASH_MAP
	#else
		#include <hash_map>
	#endif
#endif

NAMESPACE_BEGIN(QuickNet)

#ifdef __GNUC__
	using namespace __gnu_cxx;
	typedef hash_map<IUINT32, ProtocolPacket*> PacketHash;
#else
	using namespace stdext;
	typedef hash_map<IUINT32, ProtocolPacket*> PacketHash;
#endif

struct RecvSeq
{
	IUINT32 m_nSendTime;
	IUINT32 m_nSn;
};

//---------------------------------------------------------------------
// RequestRepeat
//---------------------------------------------------------------------
class RequestRepeat
{
public:
	RequestRepeat(void* user);
	~RequestRepeat();
public:
	// session 接口
	int input(ProtocolPacket *packet);
	int send(ProtocolPacket *packet);
	ProtocolPacket *recv_packet();
	int update(IUINT32 current, int rto);
	// 设置拉包缓存大小
	int set_pull_size(int size);
	void send_flag_pull(int sn);
	int get_pull_size();

	int get_skip_size();
	void set_skip_size(int size);
	/*
	* @packets  总报文数
	* @pull		被拉过包的报文数
	* @pullpkts	实际拉包消息数，当前拉包策略下(仅仅参考)
	* @lost		实际丢包数
	* @pulltimeout 拉包后超时包个数（仅仅参考）
	* @skip		连续包忽略拉包次数
	* @totalskippkt 连续包忽略拉包总个数
	*/
	void get_stat_info(int* packets, int *pull, int* pullpkts, int *lost, int *pulltimeout, int *skip, int *totalskippkt);

	void log(int mask, const char *fmt, ...);
	
public:
	// session接口
	int (*output)(ProtocolPacket *packet, void *user);
	void (*writelog)(const char *log, void *user);
	int logmask;
	
	IUINT32 m_nSendPacketCounter;		// 发包计数
	int m_nPullSize;					// 拉包缓存大小
	int m_nSkipSize;					// 拉包忽略大小
	void *m_pUser;
	IUINT32 m_nMaxPacketSn;				// 最大包大小
	IUINT32 m_nCurrent;					// 当前时间戳
	IUINT32 m_nRtt;						// 当前回射周期
	IUINT32 m_nLastRecvSn;				// 上一个接受的序列号

	std::list<ProtocolPacket*>			m_listSendPacket;		// 待发送包
	std::list<IUINT32>					m_listSendPacketSn;		// 已发送包序号
	std::list<RecvSeq>					m_listRecvData;			// 待接收数据
	std::list<RecvSeq>					m_listSecondPullPacketSn;	// 等待第2次拉包序号
	PacketHash							m_mapSendPacket;		// 已发送包字典
	PacketHash							m_mapRecvPacket;		// 接收包字典
	//std::map<IUINT32, ProtocolPacket*>	m_mapSendPacket;	// 已发送包字典
	//std::map<IUINT32, ProtocolPacket*>	m_mapRecvPacket;	// 接收包字典

	int									m_nTimesPull;			// 拉包次数
	int									m_nPacketsLost;			// 丢包次数
	int									m_nTimesRepeat;			// 重包次数
	int									m_nPacketsPullTimeout;	// 超时拉包
	int									m_nTimesSkip;			// 忽略拉包次数
	int									m_nPacketsSkip;			// 忽略拉包的总个数
	int									m_nPacketsPull;			// 拉的包数
};

#define QUICKNET_NACK_RTO_DEFAULT			300
#define QUICKNET_NACK_RTO_DEFAULT_MIN		100
#define QUICKNET_NACK_RTO_DEFAULT_MIN_X2	200
#define QUICKNET_NACK_RTO_DEFAULT_MAX		250
#define QUICKNET_NACK_RTO_DEFAULT_MAX_X2	500

#define QUICKNET_NACK_PKTNUM_PULL_SKIP		23

NAMESPACE_END(QuickNet)

#endif


