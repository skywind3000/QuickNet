#include "ProtocolBasic.h"

//#include "fec_itcp.h"
#include "NetFecCodec.h"
#include "FecPacket.h"
#include "FecCodec.h"
#include <vector>
#include "FecCodecBuf.h"
#include "../system/imemdata.h"
#include "../system/inetbase.h"

#ifdef _DEBUG_BY_LOG
#include "NetFecCodecTest.h"
#include <time.h>
#endif

bool is_zfec_debug = false;
/**
*解析过程中，根据i_curpkt,受到包序号，更新缓冲；
*/

void  update_fec_dec_buf(NetFecCodecLayer & zfec,  IUINT32 iCurPkt, int cur_k, int cur_n,  IUINT32 iCurSegBeg);
/**
*解包过程中，把受到的包iPacket,放入decoded buf;
*/
bool add_packet_fec_buf(NetFecCodecLayer & zfec,  IUINT32 iPacket,  IUINT32 iSrcPkt,  const char* buf, int size, int cur_k, int cur_n,  IUINT32 nSegBeg, int &);
/**
*把解包缓冲区中所有可用的pkts冲掉
*/
bool flush_avail_pkts(NetFecCodecLayer & zfec,void *peer,  IUINT32 lastis,  IUINT32 lastie);

/**
*调试目的，把参加fec decoding的包打印
*/
void trace_fec_dec_pkts(NetFecCodecLayer & zfec,  IUINT32 iPacket,  IUINT32 iSrcPkt, int cur_k, int cur_n,  IUINT32 nSegBeg);

/**
*设置某个为used
*/
void set_fec_dec_buf_used(NetFecCodecLayer & zfec, IUINT32 iPacket, bool bUsed);
bool is_fec_dec_buf_used(NetFecCodecLayer & zfec, IUINT32 iPacket);
/**
*更新网络lost;
*/
void update_channel_lost(NetFecCodecLayer& zfec,  IUINT32 _iRecvPkt);
void init_net_channel(NetChannel& net);

/**
*重新计算zfec的k,n;
*/
FecCodec* recalc_zfec_kn(NetFecCodecLayer & zfec)
{
	if (zfec.fec_codec == NULL)
	{
		return NULL;
	}
	FecCodec *cur_fec = get_codec_by(zfec.codecList, zfec.lost_rate);
	if (cur_fec == NULL)
	{
		cur_fec = zfec.fec_codec;
	}
    zfec.fec_codec = cur_fec;
    //printf("recalc Lost %.2f, K: %d, N: %d\n", zfec.lost_rate, cur_fec->k, cur_fec->n);
	return cur_fec;
}


IUINT32 zfec_pack_input(NetFecCodecLayer & zfec, void *outpeer, const void *data,  IUINT32 size,  IUINT32 sn)
{
	IUINT32 nRet = -1;
    if (zfec.trace)
    {
        zfec.trace->out(TRACE_UDP_BYTES,"[FEC] zfec_pack input size=%d",(int)size);
    }
	if (zfec.is_enabled == false || zfec.fec_codec == NULL)
	{
        int packed_size = 0;
        const char *p_packed_buf = pack_fec_off_tag(zfec.codec_buf, (const char*)data, (int)size, packed_size);
        if (zfec.PackOutput != NULL)
		{
            if (p_packed_buf != NULL && packed_size>0 )
            {
                if (zfec.trace) zfec.trace->out(TRACE_UDP_BYTES,"[FEC] zfec_pack  output size=%d",(int)packed_size);
                nRet = zfec.PackOutput(outpeer,(const char*)p_packed_buf,packed_size);
            }
            else
            {
                if (zfec.trace) zfec.trace->out(TRACE_UDP_BYTES,"[FEC] zfec_pack output size=%d",(int)size);
                nRet = zfec.PackOutput(outpeer,(const char*)data,size);
            }
		}
		//zfec.i_sent_pkt++;//此处会有潜在的bug;
        return nRet;
	}
	
	int cur_k = zfec.fec_codec->k;
	int cur_n = zfec.fec_codec->n;

	int ik = (zfec.i_sent_pkt - zfec.i_cur_segment_beg)%cur_n;
	if (ik < cur_k)
	{
		FecCodecHead curHead;
		curHead.sent_pkt_index = zfec.i_sent_pkt;
		curHead.src_pkt_index  = zfec.i_sent_src_pkt;
		curHead.codec_k        = cur_k;
		curHead.codec_n        = cur_n;
		curHead.ik             = ik;
		int packed_size = -1;
        int en_size = -1;
		const char* p_enc_buf    = set_fec_enc_buf(zfec.codec_buf, ik, data, size,en_size);
        if (ik == 0)
        {
            zfec.nGroupMaxPktSize = en_size;
        }
        else
        {
            zfec.nGroupMaxPktSize = std::max<int>(zfec.nGroupMaxPktSize, en_size);
        }
        const char *p_packed_buf = pack_fec_head(zfec.codec_buf, curHead, p_enc_buf, en_size, packed_size);
        //const char *p_packed_buf = pack_fec_head(zfec.codec_buf, curHead, (const char*)data, size, packed_size);
        if (zfec.PackOutput != NULL && p_packed_buf != NULL && packed_size > 0)
		{
            if (zfec.trace) 
            {
                zfec.trace->out(TRACE_UDP_BYTES,"[FEC] zfec_pack output size=%d, sn=%d",(int)packed_size, (int)zfec.i_sent_pkt, (int)zfec.i_sent_src_pkt);
            }
			nRet = zfec.PackOutput(outpeer, p_packed_buf, packed_size);
		}
		//--
		zfec.i_sent_pkt++;
		zfec.i_sent_src_pkt++;
	}
	if (ik == cur_k-1) //连发几个check packets;
	{
		void *fec_encoder = zfec.fec_codec->codec;
		for(ik = cur_k; ik<cur_n; ik++)
		{
			//发送的原始数据包序号
			IUINT32 curSentSrcPkt = zfec.i_sent_src_pkt - 1;
			//current fec codec head;
			FecCodecHead curHead;
			curHead.sent_pkt_index = zfec.i_sent_pkt;
			curHead.src_pkt_index  = curSentSrcPkt;
			curHead.codec_k        = cur_k;
			curHead.codec_n        = cur_n;
			curHead.ik             = ik;
			//==
			const char * p_check_buf  = NULL;
			int en_size = -1;
			int packed_size = -1;
            if (zfec.nGroupMaxPktSize <= 0)
            {
                zfec.nGroupMaxPktSize = zfec.max_pkt_size;
            }
			p_check_buf = get_fec_encoded_pkt(zfec.codec_buf, fec_encoder, ik, zfec.nGroupMaxPktSize, en_size);
            const char *p_packed_buf = pack_fec_head(zfec.codec_buf, curHead, p_check_buf, en_size, packed_size);
			if (zfec.PackOutput != NULL && packed_size > 0 && p_packed_buf != NULL)
			{
                if (zfec.trace) 
                {
                    zfec.trace->out(TRACE_UDP_BYTES,"[FEC] zfec_pack output size=%d, sn=%d",(int)packed_size, (int)zfec.i_sent_pkt, (int)zfec.i_sent_src_pkt);
                }
                nRet = zfec.PackOutput(outpeer,(const char*)p_packed_buf, packed_size);
			}
			zfec.i_sent_pkt++;
		}
        if (zfec.bChangeKNBaseLost)
        {
            recalc_zfec_kn(zfec);
        }
        zfec.i_cur_segment_beg = zfec.i_sent_pkt;
	}

	return nRet;
}

void set_delay_threshold(NetFecCodecLayer& zfec, float delay)
{
	zfec.delay_threshold = delay;
}

void set_transimision_state(NetFecCodecLayer& zfec, float _lostRate, float delay)
{
	zfec.lost_rate = _lostRate;
	zfec.cur_delay = delay;
}


int zfec_unpack_input(NetFecCodecLayer & zfec, void *peer, const char* buf, unsigned int size)
{
    if (zfec.trace) 
    {
        zfec.trace->out(TRACE_UDP_BYTES,"[FEC] zfec_unpack input size=%d",(int)size);
    }

	int nRet = size;
	FecCodecHead curFecHead;
	int unpacked_size = -1;

	const char* p_unpack_buf = unpack_fec_head(zfec.codec_buf, curFecHead, buf,size, unpacked_size);
	if (unpacked_size == (int)size-1 && p_unpack_buf != NULL)
	{
		if (zfec.UnpackOutput)
		{
            if (zfec.trace) zfec.trace->out(TRACE_UDP_BYTES,"[FEC] zfec_unpack output size=%d",(int)unpacked_size);
			zfec.UnpackOutput(peer, p_unpack_buf, unpacked_size, 0);
		}
		return nRet;
	}
	if (p_unpack_buf == NULL || unpacked_size < 0)
	{
		return 0;
	}

    IUINT32  i_recv_pkt = curFecHead.sent_pkt_index;
    IUINT32 curSentSrcPkt = curFecHead.src_pkt_index;
	int cur_n = curFecHead.codec_n;
	int cur_k = curFecHead.codec_k;
	int cur_ni = curFecHead.ik;
    IUINT16 size_pkt = 0;
    IUINT32 iPktCurSegBeg = i_recv_pkt - cur_ni;//当前段起始包的总体序号
	//update_channel_lost(zfec,i_recv_pkt);
	zfec.i_recv_pkt = std::max< IUINT32>(i_recv_pkt, zfec.i_recv_pkt);

	 IUINT32 iPktCurSegSrcBeg = curSentSrcPkt;//当前段起始包的原始序号
	if (cur_ni<cur_k) //原始包
	{
		iPktCurSegSrcBeg = curSentSrcPkt - cur_ni;
	}
	else   //校验包
	{
		iPktCurSegSrcBeg = curSentSrcPkt - cur_k + 1;
	}

	update_fec_dec_buf(zfec, i_recv_pkt, cur_k, cur_n, iPktCurSegBeg); //总体包序号
	bool bDec = false;
    bool bUsed = false;
	if (cur_ni < cur_k) // this is an source packets, just using it;
	{
      	const char* p_src_pkt = p_unpack_buf;
		p_src_pkt = dec_src_pkt_info(p_src_pkt, zfec.codec_buf, size_pkt);
		if (p_src_pkt == NULL)
		{
			//checksum failed;
			if (zfec.trace)
			{
				zfec.trace->out(TRACE_ERROR, "[FEC] source packet checksum failed! the packet is dropped!");
				zfec.trace->out(TRACE_ERROR, "[FEC] recv=%d, k=%d, n=%d, in=%d, isend=%d, isrc=%d,!", (int)zfec.i_recv_pkt, 
					(int)cur_k, (int)cur_n, (int)cur_ni, (int)zfec.i_sent_pkt, (int)zfec.i_sent_src_pkt);

				zfec.trace->binary(TRACE_ERROR, p_unpack_buf, unpacked_size);
			}
			return 0;
		}

		if ( zfec.is_sorted == false && zfec.UnpackOutput)
		{
            if (is_fec_dec_buf_used(zfec, i_recv_pkt) == false)
            {
                if (zfec.trace) zfec.trace->out(TRACE_UDP_BYTES,"[FEC] zfec_unpack output size=%d",(int)size_pkt);
                zfec.fec_src_count++;
                zfec.UnpackOutput(peer, p_src_pkt, size_pkt, iPktCurSegSrcBeg+cur_ni);
            }
		    bUsed = true;
		}
		if (i_recv_pkt == zfec.i_expected_packet && zfec.is_sorted)
		{
            if (zfec.UnpackOutput)
            {
                if (zfec.trace) zfec.trace->out(TRACE_UDP_BYTES,"[FEC] zfec_unpack output size=%d",(int)size_pkt);
                zfec.fec_src_count++;
                zfec.UnpackOutput(peer, p_src_pkt, size_pkt, iPktCurSegSrcBeg+cur_ni);
                bUsed = true;
            }
            //增加期望索引；
			zfec.i_expected_packet++;
			if (int(zfec.i_expected_packet - iPktCurSegBeg)%cur_n == cur_k)
			{
				zfec.i_expected_packet = iPktCurSegBeg+cur_n;
			}
		}
       nRet = unpacked_size;
	}

	
    int curGroupMaxSize = 0;
	bDec = add_packet_fec_buf(zfec, i_recv_pkt,curSentSrcPkt, p_unpack_buf, unpacked_size, cur_k, cur_n, iPktCurSegBeg,curGroupMaxSize);
    set_fec_dec_buf_used(zfec, i_recv_pkt, bUsed);
    if (!bDec && i_recv_pkt - zfec.i_expected_packet >= (IUINT32)(2*cur_n) && zfec.is_sorted )
    {
        flush_avail_pkts(zfec, peer, zfec.i_expected_packet, iPktCurSegBeg);
        zfec.i_expected_packet = iPktCurSegBeg;
    }
	if (bDec)
	{
		if (zfec.is_sorted)
		{
			flush_avail_pkts(zfec, peer, zfec.i_expected_packet, iPktCurSegBeg);
		}
        //trying to fec decode it;
		FecCodec *cur_fec = find_codec(zfec.codecList,  cur_k, cur_n);
		if (cur_fec == NULL)
		{
			return nRet;
		}
		fec_decode_pkts(zfec.codec_buf, cur_fec->codec,curGroupMaxSize);

		for( int i = 0; i<cur_n; i++)
		{
			if (i>=cur_k )
			{
				continue;
			}
			const char* p_dec_buf = get_fec_decoded_pkt(zfec.codec_buf, i);
			if (p_dec_buf == NULL)
			{
				continue;
			}
            const char* p_src_pkt = dec_src_pkt_info(p_dec_buf, zfec.codec_buf, size_pkt);
            if (p_src_pkt == NULL)
            {
				//checksum failed;
				if (zfec.trace)
				{
					zfec.trace->out(TRACE_ERROR, "[FEC] [Dropped] decoded packet checksum failed! size=%d, groupsize=%d,", (int)size_pkt, (int)curGroupMaxSize);
					zfec.trace->out(TRACE_ERROR, "[FEC] recv=%d, k=%d, n=%d, in=%d, isend=%d, isrc=%d,!", (int)zfec.i_recv_pkt, 
						(int)cur_k, (int)cur_n, (int)i, (int)zfec.i_sent_pkt, (int)zfec.i_sent_src_pkt);
					trace_fec_dec_pkts(zfec, i_recv_pkt, curSentSrcPkt, cur_k, cur_n,  iPktCurSegBeg);
                    zfec.trace->out(TRACE_ERROR, "[FEC] -------------------packets in decoding---------------------");
					zfec.trace->binary(TRACE_ERROR, p_dec_buf, curGroupMaxSize);
				}
                continue;
            }

			if ( zfec.is_sorted == false && zfec.UnpackOutput)
			{
                if (is_fec_dec_buf_used(zfec, iPktCurSegBeg+i) == false)
                {
                    if (zfec.trace) zfec.trace->out(TRACE_UDP_BYTES,"[FEC] zfec_unpack output size=%d",(int)size_pkt);
                    zfec.UnpackOutput(peer, p_src_pkt, size_pkt, iPktCurSegSrcBeg+i);
                    set_fec_dec_buf_used(zfec, iPktCurSegBeg+i, true);
                    zfec.fec_src_count++;
                    zfec.fec_restore_count++;
                }
				
			}

			if (iPktCurSegBeg+i >= zfec.i_expected_packet && zfec.is_sorted)
			{
                if (zfec.UnpackOutput && is_fec_dec_buf_used(zfec, iPktCurSegBeg+i) == false)
                {
                    if (zfec.trace) zfec.trace->out(TRACE_UDP_BYTES,"[FEC] zfec_unpack output size=%d",(int)size_pkt);
                    zfec.UnpackOutput(peer, p_src_pkt, size_pkt, iPktCurSegSrcBeg+i);
                    set_fec_dec_buf_used(zfec, iPktCurSegBeg+i, true);
                    zfec.fec_src_count++;
                    zfec.fec_restore_count++;
                }
                //增加期望索引；
				zfec.i_expected_packet = iPktCurSegBeg+i+1;
				if (int(zfec.i_expected_packet - iPktCurSegBeg)%cur_n == cur_k)
				{
					zfec.i_expected_packet = iPktCurSegBeg+cur_n;
				}
			}
            set_fec_dec_buf_used(zfec, i_recv_pkt, bUsed);
		}//for
		//当前段已经全部就绪，无论之前是否就绪，直接跳过。
		nRet = nRet - sizeof(IINT32)*3;
	}// bDec;
	return nRet;
}



void enable_zfec(NetFecCodecLayer & zfec, bool right)
{
	zfec.is_enabled = right;
}

bool is_zfec_enabled(const NetFecCodecLayer & zfec)
{
	return zfec.is_enabled;
}

void enable_zfec_dynkn(NetFecCodecLayer & zfec, bool right)
{
    zfec.bChangeKNBaseLost = right;
}

bool is_zfec_dynkn(const NetFecCodecLayer & zfec)
{
    return zfec.bChangeKNBaseLost;
}

bool zfec_flush_avail_unpack_pkts(NetFecCodecLayer & zfec,void *peer)
{
	return flush_avail_pkts(zfec, peer, zfec.i_expected_packet, zfec.dec_buf_ipkt_range.second);
}

bool is_zfec_avail_unpack_pkts(NetFecCodecLayer & zfec)
{
	return zfec.i_expected_packet>0 && zfec.i_expected_packet< zfec.i_recv_pkt;
}
/**
*把所有可用的pkts冲掉
*/
bool flush_avail_pkts(NetFecCodecLayer & zfec,void *peer,  IUINT32 lastis,  IUINT32 lastie)
{
	bool bRet = false;
	//flush out all of the available packets
	if (lastie > lastis && lastis >= zfec.dec_buf_ipkt_range.first && lastis < zfec.dec_buf_ipkt_range.second
		&& lastie > zfec.dec_buf_ipkt_range.first && lastie <= zfec.dec_buf_ipkt_range.second)
	{
		for (IUINT32 i = lastis; i<lastie; i++)
		{
			int ck = i - zfec.dec_buf_ipkt_range.first;
			if (zfec.dec_pkts_buf[ck].IsValid() && zfec.dec_pkts_buf[ck].bSourcePkt)
			{
				const char* p_cur_buf = zfec.dec_pkts_buf[ck].FecBuf;
                IUINT16 cur_pkt_size = 0;
                const char* p_src_pkt = dec_src_pkt_info(p_cur_buf, zfec.codec_buf, cur_pkt_size);
                if (p_src_pkt == NULL)
                {
                    //error occues;
                    continue;
                }

				/*memcpy(&cur_pkt_size, p_cur_buf, sizeof(IINT32) );*/
				IUINT32 cur_src_index = zfec.dec_pkts_buf[ck].i_source_pkt ;
				if (zfec.UnpackOutput && is_fec_dec_buf_used(zfec, i) == false)
				{
                    if (zfec.trace) zfec.trace->out(TRACE_UDP_BYTES,"[FEC] zfec_unpack output size=%d",(int)cur_pkt_size);
                    zfec.fec_src_count++;
					zfec.UnpackOutput(peer, p_src_pkt, cur_pkt_size, cur_src_index);
                    set_fec_dec_buf_used(zfec, i, true);
				}
                zfec.dec_pkts_buf[ck].Reset(zfec.dec_pkts_buf[ck].MaxBufSize);
				bRet = true;
			}
		}
	}
	return bRet;
}

void trace_fec_dec_pkts(NetFecCodecLayer & zfec,  IUINT32 iPacket,  IUINT32 iSrcPkt, int cur_k, int cur_n,  IUINT32 nSegBeg)
{
	int iValid = 0;
	bool bAllSrcPktAvail = true;
	int maxSize = 0;
	for( int i =0; iValid<cur_k && i<cur_n; i++)
	{
		int ck = nSegBeg - zfec.dec_buf_ipkt_range.first + i;
		if (ck < 0 || ck>=(int)zfec.dec_pkts_buf.size())
		{
			continue;
		}
		if (zfec.dec_pkts_buf[ck].IsValid() && zfec.dec_pkts_buf[ck].iPacket ==  nSegBeg+i)
		{
			//checksum failed;
			if (zfec.trace)
			{
				zfec.trace->out(TRACE_ERROR, "[FEC] decoded packet! k=%d, ipkt=%d, isrc=%d, size=%d,group=%d", i, zfec.dec_pkts_buf[ck].iPacket, nSegBeg+i, 
					(int)zfec.dec_pkts_buf[ck].BufSize, (int)maxSize);
				zfec.trace->binary(TRACE_ERROR, zfec.dec_pkts_buf[ck].FecBuf, zfec.dec_pkts_buf[ck].BufSize);
			}
			if (iValid == 0)
			{
				maxSize = zfec.dec_pkts_buf[ck].BufSize;
			}
			else
			{
				maxSize = std::max<int>(maxSize, zfec.dec_pkts_buf[ck].BufSize);
			}
			iValid++;
			if (ck >= cur_k)
			{
				bAllSrcPktAvail = false;
			}
		}
	}
}



bool add_packet_fec_buf(NetFecCodecLayer & zfec,  IUINT32 iPacket,  IUINT32 iSrcPkt,  const char* buf, int size, int cur_k, int cur_n,  IUINT32 nSegBeg, int& maxSize)
{
	bool bRet = false;
	if (iPacket >= zfec.dec_buf_ipkt_range.first && iPacket < zfec.dec_buf_ipkt_range.second)
	{
		zfec.dec_pkts_buf[(iPacket-zfec.dec_buf_ipkt_range.first)].SetPacket(buf, size);
		zfec.dec_pkts_buf[(iPacket-zfec.dec_buf_ipkt_range.first)].iPacket = iPacket;
		zfec.dec_pkts_buf[(iPacket-zfec.dec_buf_ipkt_range.first)].bSourcePkt = iPacket-nSegBeg < (IUINT32)cur_k;
		zfec.dec_pkts_buf[(iPacket-zfec.dec_buf_ipkt_range.first)].i_source_pkt = iSrcPkt;
	}
	else
	{
		return bRet;
	}

	int iValid = 0;
	bool bAllSrcPktAvail = true;
	reset_fec_dec_buf(zfec.codec_buf);

	for( int i =0; iValid<cur_k && i<cur_n; i++)
	{
		int ck = nSegBeg - zfec.dec_buf_ipkt_range.first + i;
        if (ck < 0 || ck>=(int)zfec.dec_pkts_buf.size())
        {
            continue;
        }
		if (zfec.dec_pkts_buf[ck].IsValid() && zfec.dec_pkts_buf[ck].iPacket ==  nSegBeg+i)
		{
			set_fec_dec_buf(zfec.codec_buf, iValid, zfec.dec_pkts_buf[ck].FecBuf,  zfec.dec_pkts_buf[ck].BufSize, i);
            if (iValid == 0)
            {
                maxSize = zfec.dec_pkts_buf[ck].BufSize;
            }
            else
            {
                maxSize = std::max<int>(maxSize, zfec.dec_pkts_buf[ck].BufSize);
            }
			iValid++;
			if (ck >= cur_k)
			{
				bAllSrcPktAvail = false;
			}
		}
	}

	if (iValid == cur_k && bAllSrcPktAvail == false )
	{
		bRet = true;
	}
	return bRet;
}

/**
*解析过程中，根据i_curpkt,受到包序号，更新缓冲；
*/
void  update_fec_dec_buf(NetFecCodecLayer & zfec,  IUINT32 iCurPkt, int cur_k, int cur_n,  IUINT32 iCurSegBeg)
{
	 IUINT32 nCurSegEnd = iCurSegBeg + cur_n;
	if (nCurSegEnd > zfec.dec_buf_ipkt_range.second)
	{
		int ns = int(nCurSegEnd-zfec.dec_buf_ipkt_range.second);
		for( int is = ns; is < int(zfec.dec_buf_ipkt_range.second - zfec.dec_buf_ipkt_range.first); is++)
		{
			zfec.dec_pkts_buf[is - ns] = zfec.dec_pkts_buf[is];
			zfec.dec_pkts_buf[is].Reset(zfec.dec_pkts_buf[is].MaxBufSize);
		}
		zfec.dec_buf_ipkt_range.first  = zfec.dec_buf_ipkt_range.first + ns;
		zfec.dec_buf_ipkt_range.second = zfec.dec_buf_ipkt_range.second + ns;
	}
}

bool is_fec_dec_buf_used(NetFecCodecLayer & zfec, IUINT32 iPacket)
{
    bool bRet = false;
    if (iPacket >= zfec.dec_buf_ipkt_range.first && iPacket < zfec.dec_buf_ipkt_range.second)
    {
        bRet = zfec.dec_pkts_buf[(iPacket-zfec.dec_buf_ipkt_range.first)].bUsed;
    }
    return bRet;
}

void set_fec_dec_buf_used(NetFecCodecLayer & zfec, IUINT32 iPacket, bool bUsed)
{
    if (iPacket >= zfec.dec_buf_ipkt_range.first && iPacket < zfec.dec_buf_ipkt_range.second)
    {
        zfec.dec_pkts_buf[(iPacket-zfec.dec_buf_ipkt_range.first)].bUsed = bUsed;
    }
}

bool is_zfec_packet(NetFecCodecLayer & zfec, const char* packet, int pktsize)
{
    return is_fec_buf(packet, pktsize);
}

int get_zfec_kn(const NetFecCodecLayer & zfec, int& k, int& n) 
{
    int iRet = -1;
    if (zfec.fec_codec != NULL)
    {
        k = zfec.fec_codec->k;
        n = zfec.fec_codec->n;
        iRet = n;
    }
    return iRet;
}

int set_zfec_kn(NetFecCodecLayer & zfec, int k, int n, bool add_new )
{
	int iRet = -1;
	if (k < 0 || n < 0 || k>n)
	{
		return iRet;
	}
	FecCodec *cur_fec = find_codec(zfec.codecList, k,n);
	if (cur_fec)
	{
		zfec.fec_codec = cur_fec;
	}
	else
	{
		if (add_new )
		{
			zfec.fec_codec = add_new_codec(zfec.codecList, k, n);
		}
	}
	return zfec.fec_codec == NULL? -2 : 0;
}

void init_zfec_layer(NetFecCodecLayer & layer, int max_pkt_size, int fec_buf_limit, int k_max )
{
	//initialize the fec coded buf's
	init_fec_buf(layer.codec_buf, max_pkt_size, k_max);
	layer.codec_buf.is_send_checksum = true;
	layer.trace = NULL;
	layer.codec_buf.is_checksum = false;
	//
	layer.fec_codec = NULL;
	layer.max_pkt_size = max_pkt_size;
	layer.i_sent_pkt  = 0;
	layer.i_recv_pkt = 0;
	layer.i_sent_src_pkt = 0;
	layer.i_cur_segment_beg = 0;
	layer.i_expected_packet = 0;
	layer.n_fec_item_limit = fec_buf_limit;
	layer.dec_buf_ipkt_range = std::pair< IUINT32, IUINT32>(0,fec_buf_limit);
	layer.dec_pkts_buf.reserve(fec_buf_limit);
	layer.delay_threshold  = 2500;//(ms)
	layer.lost_rate = 0.20f;
	layer.cur_delay = 80;
	layer.is_sorted = true;
    layer.bChangeKNBaseLost = false;
    layer.fec_restore_count = 0;
    layer.fec_src_count = 0;
	init_net_channel(layer.chnl_info);
	layer.PackOutput = NULL;
	layer.UnpackOutput = NULL;
	if (fec_buf_limit < int(layer.dec_pkts_buf.size()) )
	{
		int n = int(layer.dec_pkts_buf.size())-fec_buf_limit;
		for (int i = 0; i<n; i++)
		{
			if (layer.dec_pkts_buf[i].FecBuf != NULL)
			{
				free ( layer.dec_pkts_buf[i].FecBuf);
			}
		}
		layer.dec_pkts_buf.erase(layer.dec_pkts_buf.begin(), layer.dec_pkts_buf.begin()+n);
	}
	if (fec_buf_limit > int(layer.dec_pkts_buf.size()))
	{
		int n = fec_buf_limit - int(layer.dec_pkts_buf.size());
		for (int i = 0; i<n; i++)
		{
			layer.dec_pkts_buf.push_back( FecPacket(max_pkt_size+16) );
		}
	}
	for (int i = 0; i< int(layer.dec_pkts_buf.size()); i++)
	{
		layer.dec_pkts_buf[i].Reset(max_pkt_size+16);
	}
	layer.is_enabled = false;
#ifdef _DEBUG_BY_LOG
	layer.tester = NULL;
#endif
}

void release_z_fec_layer(NetFecCodecLayer & layer)
{
    release_all_codec(layer.codecList );
	release_fec_enc_buf(layer.codec_buf);
	release_fec_dec_buf(layer.codec_buf);
    layer.fec_restore_count = 0;
    layer.fec_src_count = 0;
	for (std::vector<FecPacket>::iterator it = layer.dec_pkts_buf.begin(); it != layer.dec_pkts_buf.end(); it++)
	{
		if (it->FecBuf != NULL)
		{
			free ( it->FecBuf);
		}
	}
	layer.dec_pkts_buf.clear();
    
}

void enable_zfec_debug(bool right)
{
	is_zfec_debug = right;
}

float calc_zfec_channel_lost(NetFecCodecLayer& zfec)
{
	return zfec.chnl_info.fChannelLost;
}

void init_net_channel(NetChannel& chanl)
{
	chanl.dwLastTick = 0;
	chanl.nSendPkt = 0;
	chanl.nBegSentPkt = 0;
	chanl.nRecvPkt = 0;
	chanl.fChannelLost = 0.0f;

}


void update_channel_lost(NetFecCodecLayer& zfec,  IUINT32 i_recv_pkt)
{
	NetChannel &chnl_info = zfec.chnl_info;
	if (chnl_info.dwLastTick == 0)
	{
		chnl_info.dwLastTick = iclock();
	}

	IUINT32 dwCurTick = iclock();
	//if (dwCurTick - chnl_info.dwLastTick >= 20000)
	//{
	//	chnl_info.nRecvPkt = 0;
	//	chnl_info.nSendPkt = i_recv_pkt;
	//	chnl_info.nBegSentPkt = i_recv_pkt;
	//	chnl_info.dwLastTick = dwCurTick;
	//	return;
	//}
	if (dwCurTick - chnl_info.dwLastTick >= 20000 || chnl_info.nRecvPkt == 0)
	{
		if (chnl_info.nSendPkt > chnl_info.nBegSentPkt)
		{
			chnl_info.fChannelLost = 1.0f - float(chnl_info.nRecvPkt)/float(chnl_info.nSendPkt - chnl_info.nBegSentPkt+1);
		}
		
		chnl_info.nRecvPkt = 1;
		chnl_info.nSendPkt = i_recv_pkt;
		chnl_info.nBegSentPkt = i_recv_pkt;
		
		chnl_info.dwLastTick = dwCurTick;
	}
	else
	{
		chnl_info.nSendPkt = std::max< IUINT32>(chnl_info.nSendPkt, i_recv_pkt);
		chnl_info.nRecvPkt++;
	}
}

bool is_sorted_zfec(const NetFecCodecLayer & zfec)
{
	return zfec.is_sorted;

}
void enable_sorted_zfec(NetFecCodecLayer & zfec, bool right)
{
	zfec.is_sorted = right;
}



