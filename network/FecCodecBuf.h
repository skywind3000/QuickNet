
#ifndef _AUDIOMAIN_NETWORK_FEC_CODEC_BUF_H_
#define _AUDIOMAIN_NETWORK_FEC_CODEC_BUF_H_

#include "../system/imemdata.h"

const unsigned char  tagFecPktHead = 0xEC;
const unsigned char  tagFecPktHeadCheksum = 0xED;
const unsigned char  tagFecOFFTag  = 0x13;
typedef struct tagFecCodecHead
{
	IUINT32 sent_pkt_index; //当前包“总体”序号 4 BYTES 0 to 4,294,967,295
	IUINT32 src_pkt_index;  //当前包“原始数据”序号，校验包 4 BYTES 0 to 4,294,967,295
	unsigned char  codec_n;        //fec 的nl 1*BYTE, 0-255;
	unsigned char  codec_k;        //fec 的k   1*BYTE, 0-255;
	unsigned char  ik;              //fec段内的序号，可以用来确定是否校验包 1*BYTE, 0-255;
}FecCodecHead;

typedef struct tagFecCodecBuf
{
    int enc_pkt_size; //the size packed with head.
    int enc_kmax;
    bool is_checksum; //recv
	bool is_send_checksum;
  	//--fec_encoder--
	char **fec_en_buf; // k*PacketBytes
	char *sent_buf;
	char *en_check_pkt;
	//--- FEC decoder --
    int dec_pkt_size; //the size packed with head.
    int dec_kmax;
	char **fecDecoderBuf;
	int *fecDecoderIndices;
	char *dec_buf;
	char *dec_check_pkt;
}FecCodecBuf;

#ifdef __cplusplus
extern "C" {
#endif

/**
*设定fec-encode buf ik,原始包
*/
const char* set_fec_enc_buf(FecCodecBuf& fecBuf, int ik, const void* pBuf,  int size, int& en_size);
const char* set_fec_dec_buf(FecCodecBuf& fecBuf, int index, const void* pBuf,  int size, int ik);

/**
*
*/
bool is_fec_buf(const char* pbuf, int size);
void reset_fec_dec_buf(FecCodecBuf& fecBuf);
/**
*获取fec-encoded的包
*/
const char* get_fec_encoded_pkt(FecCodecBuf& fecBuf, void *p_fec_codec, int ik, int groupMaxPktSize, int& en_size);

//输入p_dec_buf, fec dec包，(参加fec decode运算的包）
//output the source data in packet, decode the pkt size. Check packet by checksum;
const char* dec_src_pkt_info(const char* p_dec_buf, FecCodecBuf& fecBuf, IUINT16& sizepkt);

int fec_decode_pkts(FecCodecBuf& fecBuf, void *p_fec_codec,int maxsize);
/**
*获取fec-decoded后，第ik个包
*/
const char* get_fec_decoded_pkt(FecCodecBuf& fecBuf, int ik);
/**
*/
const char* pack_fec_off_tag(FecCodecBuf& fecBuf,const char* p_buf, int buf_size, int& packed_size);
const char* pack_fec_head(FecCodecBuf& fecBuf, const FecCodecHead& fec_head,  const char* p_buf, int buf_size, int& packed_size);
/**
*p_buf,
*/
const char* unpack_fec_head(FecCodecBuf& fecBuf, FecCodecHead& fec_head,  const char* p_buf, int buf_size, int& packed_size);


void release_fec_enc_buf(FecCodecBuf& fecBuf);
//-- FEC decode--
void release_fec_dec_buf(FecCodecBuf& fecBuf);

void init_fec_buf(FecCodecBuf& fecBuf, int _max_pkt_size, int _kmax);
void release_fec_buf(FecCodecBuf& fecBuf);
/**
*p_buf: IN 包；
*fec_head OUT
*return: 去掉包头后的paket；
*/
const char* unpack_fec_head(FecCodecBuf& fecBuf, FecCodecHead& fec_head, const char* p_buf, int buf_size, int& unpacked_size);


#ifdef __cplusplus
}
#endif


#endif



