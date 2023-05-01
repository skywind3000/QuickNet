
#ifndef _AUDIOMAIN_NETWORK_NET_FEC_CODEC_H_
#define _AUDIOMAIN_NETWORK_NET_FEC_CODEC_H_

#include "FecPacket.h"
#include <vector>
#include "FecCodec.h"
#include "FecCodecBuf.h"
#include "ProtocolBasic.h"
//#define  _DEBUG_BY_LOG

#ifdef _DEBUG_BY_LOG
class NetFecCodecTest;
#endif


typedef struct tagNetChannel
{
	unsigned int dwLastTick;
	 IUINT32 nSendPkt;
	 IUINT32 nBegSentPkt;	
	int nRecvPkt;
	float fChannelLost;
}NetChannel;

typedef struct tagNetFecCodec
{
	FecCodec* fec_codec; //当前正在使用的fec_codec;
    IUINT32 i_sent_pkt; //发送包索引号，（包括check packets)
    IUINT32 i_sent_src_pkt; //已经发送的原始数据包个数 （excludes check packet, Only source packets;
    IUINT32 i_expected_packet; //
    IUINT32 i_recv_pkt; //收到包的最大总体序号。
	int max_pkt_size;
    IUINT32 i_cur_segment_beg;
    int  nGroupMaxPktSize; //the max pkt size in current group; 
    bool bChangeKNBaseLost;
	
	int n_fec_item_limit; //fec buf的上限
	/**
	*fec解析buf的在发送序列的起始和终止位置。
	*/
	std::pair< IUINT32, IUINT32> dec_buf_ipkt_range;
	std::vector<FecPacket> dec_pkts_buf;
	bool is_enabled;
	/**
	*zfec包的控制
	*/
	float delay_threshold; //延迟阈值，用于限制n;
	float lost_rate; //当前网络丢包率,用于确定k;
	float cur_delay; //当前网络包的延迟

	bool is_sorted; //是否有序输出。

	FecCodecBuf  codec_buf; //允许同一个进程有个zfec stack;
    /***
    *按照冗余百分比(1- float(k)/float(n))从小到大排列。
    */
    FecCodecList codecList; 

	QuickNet::Trace *trace;
#ifdef _DEBUG_BY_LOG
	NetFecCodecTest *tester;
#endif
	NetChannel chnl_info;

    IINT32 fec_src_count;
    IINT32 fec_restore_count;
	/**
	*Fec codec output:
	*Encode:  to lower level to sending encoded packet.
	*/
	int (*UnpackOutput)(void * p, const char* packet, unsigned int sizepkt,  IUINT32 i_src_pkt);
	int (*PackOutput)(void * p, const char* packet, unsigned int sizepkt);
}NetFecCodecLayer;



#ifdef __cplusplus
extern "C" {
#endif

void init_zfec_layer(NetFecCodecLayer & layer, int max_pkt_size, int fec_buf_limit, int k_max );
void release_z_fec_layer(NetFecCodecLayer & layer);


bool is_zfec_packet(NetFecCodecLayer & zfec, const char* packet, int pktsize);



/**
*设定当前的网络状况
*lostRate丢包率；
*delay,当前网络延迟情况，可以用RTT值；
*/
void set_transimision_state(NetFecCodecLayer& zfec, float _lostRate, float delay);
/**
*设定允许fec层的延迟阈值
*/
void set_delay_threshold(NetFecCodecLayer& zfec, float delay);

/**
*计算zfec层统计丢包率
*/
float calc_zfec_channel_lost(NetFecCodecLayer& zfec);

/**
*设定k,n 
Param: add_new_codec? true: when failed to find the codec, new one; else, return -2;
*if successs, return 0; else return:
-1(wrong parameter) -2, failed get codecs;
*/
int set_zfec_kn(NetFecCodecLayer & zfec, int k, int n, bool add_new_codec);

int get_zfec_kn(const NetFecCodecLayer & zfec, int& k, int& n) ;


/**
*Fec codec.
*Encode: pack the "data" with fec-head and check packets and send them out by zfec::PackOutput.
*sn: pachet sequence no.
*/
 IUINT32 zfec_pack_input(NetFecCodecLayer & zfec, void *outpeer, const void *data,  IUINT32 size,  IUINT32 sn);


/**
*Fec codec. recv data from lower level to decode the packet;
*Decode: recv data from lower level to decode the packet, 
*parse the fec header, transfer the content(excluded fec head) to high level by zfec::UnpackOutput;
*/
int zfec_unpack_input(NetFecCodecLayer & zfec, void* p, const char* packet, unsigned int sizepkt);

/**
*把解包缓冲区中所有可用的pkts冲掉
*/
bool zfec_flush_avail_unpack_pkts(NetFecCodecLayer & zfec,void *peer);

/**
*是否还有没有使用的pkt
*/
bool is_zfec_avail_unpack_pkts(NetFecCodecLayer & zfec);

/**
*收到结果是否要求排序
*/
bool is_sorted_zfec(const NetFecCodecLayer & zfec);
void enable_sorted_zfec(NetFecCodecLayer & zfec, bool right);

/**
*
*/
void enable_zfec(NetFecCodecLayer & zfec, bool right);
bool is_zfec_enabled(const NetFecCodecLayer & zfec);
void enable_zfec_dynkn(NetFecCodecLayer & zfec, bool right);
bool is_zfec_dynkn(const NetFecCodecLayer & zfec);

void enable_zfec_debug(bool right);

#ifdef __cplusplus
};
#endif

#endif


