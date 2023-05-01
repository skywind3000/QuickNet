
#ifndef _AUDIOMAIN_NETWORK_FEC_CODEC_H_
#define _AUDIOMAIN_NETWORK_FEC_CODEC_H_
#include <map>
typedef struct tagFecCodec
{
	int k;
	int n;
	void *codec;
}FecCodec;
typedef std::map<float, FecCodec*> FecCodecList;

#ifdef __cplusplus
	extern "C" {
#endif

        /**
        *遍历链表，找到相应的codec
        */
        FecCodec* find_codec(FecCodecList& codecList,  int k,int n);
/**
*根据冗余度找到最合适的codec
*参数：冗余度, == 丢包率 == 1-k/n;
*/
FecCodec* get_codec_by(FecCodecList& codecList, float lostRate);

/**
*新建一个相应的codec,插入链表，返回链表头
*此操作必定会增加链表长度
*/
FecCodec* add_new_codec(FecCodecList& codecList,  int k, int n);
//
int get_codec_count(FecCodecList& codecList );
//
FecCodec* get_codec(FecCodecList& codecList,  int i);

/**
 *删除整个链表；
 *返回链表长度
 */
void release_all_codec(FecCodecList& codecList);

#ifdef __cplusplus
}
#endif


#endif




