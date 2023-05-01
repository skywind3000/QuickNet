#include "ProtocolBasic.h"

#include <string>
#include <vector>

#include "FecCodecBuf.h"
#include <memory.h>
#include <vector>
#include "../system/imemdata.h"

extern "C"
{
#include "../system/fec.h"
}

int getPackedPktSize(int size)
{
    if (size <0)
    {
        return 0;
    }
    int headSize = sizeof(FecCodecHead);
    int tagSize = sizeof(IUINT32);
    return size + sizeof(IUINT32) + headSize + tagSize;
}
void realloc_fec_buf(FecCodecBuf& fecBuf, int pkt_size, int enc_kmax, int dec_pkt_size, int dec_kmax);

//给destbuf中加上(pBuf,size)计算的checksum，
char* add_checksum(char* pDestBuf, const char* pBuf, int size)
{
    if (pBuf == NULL || pDestBuf == NULL )
    {
        return NULL;
    }
    IUINT32 checksum = icrypt_checksum(pBuf, size);
    IUINT16 uCheckSum = IUINT16( checksum& 0x0000FFFF);
    pDestBuf = iencode16u_lsb(pDestBuf, (IUINT16)uCheckSum);
    return pDestBuf;
}

//把大小为sizepkt的p_dec_buf，做checksum,并返回内容。
const char* rm_checksum(const char* p_dec_buf, int sizepkt)
{
	if (p_dec_buf == NULL)
	{
		return NULL;
	}
	const char* p_src_pkt = p_dec_buf;

	IUINT16 uCheckSum = 0;
	p_src_pkt = idecode16u_lsb(p_src_pkt,&uCheckSum);
	IUINT32 checksum = icrypt_checksum(p_src_pkt, sizepkt-2);
	IUINT16 uSrcCheckSum = IUINT16( checksum& 0x0000FFFF);
	if (uCheckSum != uSrcCheckSum)
	{
		fprintf(stderr, "[FEC] rm_checksum failed! %d vs %d, size=%d",(int)uCheckSum, (int)uSrcCheckSum, sizepkt);
		fflush(stderr);
		return NULL;
	}
	return p_src_pkt;
}


//把传输内容，放入fec 编码缓冲；
//返回缓冲区指针，
const char* set_fec_enc_buf(FecCodecBuf& fecBuf, int ik, const void* pBuf,  int size, int& en_size)
{
    int packedSize = getPackedPktSize(size); 
    if (ik > fecBuf.enc_kmax || (int)(packedSize) > (int)fecBuf.enc_pkt_size)
    {
        realloc_fec_buf(fecBuf, packedSize, std::max<int>(ik,fecBuf.enc_kmax), fecBuf.dec_pkt_size, fecBuf.dec_kmax);
    }

    if (ik >= fecBuf.enc_kmax || (int)(packedSize) > (int)fecBuf.enc_pkt_size)
    {
        en_size = -1;
        return NULL;
    }

    const char* p_enc_buf = NULL;

	if (pBuf != NULL && fecBuf.fec_en_buf != NULL)
	{
        memset(fecBuf.fec_en_buf[ik], 0, fecBuf.enc_pkt_size);
		
        int nHeadItem = 0;
        iencode16u_lsb(fecBuf.fec_en_buf[ik], (IUINT16)size);
        nHeadItem +=1;
        if (fecBuf.is_send_checksum)
        {
            add_checksum(fecBuf.fec_en_buf[ik]+nHeadItem*sizeof(IUINT16), (const char*)pBuf, size);
            nHeadItem +=1;
        }
        memcpy((void*)(fecBuf.fec_en_buf[ik]+nHeadItem*sizeof(IUINT16)), (void*)pBuf, size);
		p_enc_buf = fecBuf.fec_en_buf[ik];
		en_size = size + nHeadItem*sizeof(IUINT16);
	}
	else
	{
		en_size = 0;
	}
	return p_enc_buf;
}



//输入p_dec_buf, fec dec包，(参加fec decode运算的包）
//output the source data in packet, decode the pkt size. Check packet by checksum;
const char* dec_src_pkt_info(const char* p_dec_buf, FecCodecBuf& fecBuf, IUINT16& sizepkt)
{
    if (p_dec_buf == NULL)
    {
        return NULL;
    }
    const char* p_src_pkt = p_dec_buf;
    p_src_pkt = idecode16u_lsb(p_src_pkt,&sizepkt);
    if (sizepkt >= fecBuf.dec_pkt_size)
    {
		fprintf(stderr, "[FEC]  packet size erro!, size=%x",  sizepkt);
		fflush(stderr);
        return NULL;
    }
    if (fecBuf.is_checksum)
    {
        p_src_pkt = rm_checksum(p_src_pkt, sizepkt+2);
		if (p_src_pkt == NULL)
		{
			fprintf(stderr, "[FEC] source packet checksum failed!, size=%d",  sizepkt);
			fflush(stderr);
		}
    }
    return p_src_pkt;
}



const char* get_fec_encoded_pkt(FecCodecBuf& fecBuf, void *p_fec_codec, int ik, int groupMaxPktSize, int& en_size)
{
	const char* p_enc_buf = NULL;
	if (p_fec_codec == NULL)
	{
		en_size = -1;
		return NULL;
	}

	if (fecBuf.en_check_pkt == NULL)
	{
		return NULL;
	}

	fec_encode(p_fec_codec, (unsigned char**)(fecBuf.fec_en_buf), (unsigned char*)(fecBuf.en_check_pkt), ik, groupMaxPktSize);
	p_enc_buf = fecBuf.en_check_pkt;
	en_size = groupMaxPktSize;

	return p_enc_buf;
}

//把传输内容，放入fec 解码缓冲；
//返回缓冲区指针，
const char* set_fec_dec_buf(FecCodecBuf& fecBuf, int index, const void* pBuf,  int size, int ik)
{
    if (ik > fecBuf.dec_kmax || size > (int)fecBuf.dec_pkt_size)
    {
        realloc_fec_buf(fecBuf, fecBuf.enc_pkt_size, fecBuf.enc_kmax, size, std::max<int>(ik,fecBuf.dec_kmax));
    }
    if (index >= fecBuf.dec_kmax || ik >= fecBuf.dec_kmax || size > fecBuf.dec_pkt_size)
    {
        return NULL;
    }

    const char* p_dec_buf = NULL;
	if (pBuf != NULL && fecBuf.fecDecoderBuf != NULL)
	{
        memset(fecBuf.fecDecoderBuf[index], 0, fecBuf.dec_pkt_size);
		memcpy(fecBuf.fecDecoderBuf[index], pBuf, size);
		fecBuf.fecDecoderIndices[index] = ik;
		p_dec_buf = fecBuf.fecDecoderBuf[index];
	}
	return p_dec_buf;
}

void reset_fec_dec_buf(FecCodecBuf& fecBuf)
{
    if (fecBuf.fecDecoderIndices == NULL)
	{
		return;
	}
	memset(fecBuf.fecDecoderIndices, -1, fecBuf.dec_kmax);
	for (int i = 0; i<fecBuf.dec_kmax; i++)
	{
		memset(fecBuf.fecDecoderBuf[i], 0, fecBuf.dec_pkt_size);
	}

	return;
}

int fec_decode_pkts(FecCodecBuf& fecBuf, void *p_fec_codec, int maxSize)
{
	int iRet = -1;
	if (p_fec_codec == NULL || maxSize<=0)
	{
		return iRet;
	}
    iRet = fec_decode(p_fec_codec, (unsigned char**)fecBuf.fecDecoderBuf, fecBuf.fecDecoderIndices, maxSize);
	return iRet;
}

/**
*获取fec-decoded后，第ik个包
*/
const char* get_fec_decoded_pkt(FecCodecBuf& fecBuf, int ik)
{
	if (ik >= fecBuf.dec_kmax)
    {
        return NULL;
    }

    const char* p_dec_buf = NULL;
	if (fecBuf.fecDecoderBuf != NULL)
	{
		p_dec_buf = fecBuf.fecDecoderBuf[ik];
	}
	return p_dec_buf;
}


bool is_fec_buf(const char* pbuf, int size)
{
    IUINT8 curtag = 0x0000;
    if (pbuf != NULL && (unsigned int)(size) >= sizeof(IUINT8))
    {
        idecode8u(pbuf, &curtag);
    }
    return (curtag == tagFecPktHead)||(curtag == tagFecPktHeadCheksum);
}

const char* pack_fec_off_tag(FecCodecBuf& fecBuf,const char* p_buf, int buf_size, int& packed_size)
{
    const char* p_pack_buf = NULL;
    if (p_buf == NULL || buf_size<0)
    {
        packed_size = -1;
        return NULL;
    }
    int packedSize = getPackedPktSize(buf_size); 
    if ((packedSize) > (int)fecBuf.enc_pkt_size)
    {
        realloc_fec_buf(fecBuf, packedSize, fecBuf.enc_kmax, fecBuf.dec_pkt_size, fecBuf.dec_kmax);
    }
    if (buf_size > fecBuf.enc_pkt_size)
    {
        packed_size = -1;
        return NULL;
    }

    if (fecBuf.sent_buf != NULL)
    {
        int offset = 0;
        char* pEncodeBuf = fecBuf.sent_buf;
        memset(fecBuf.sent_buf,0, fecBuf.enc_pkt_size); //似乎没必要
        pEncodeBuf = iencode8u(pEncodeBuf, tagFecOFFTag);
        offset += 1;
      
        memcpy(pEncodeBuf, p_buf, buf_size);
        packed_size = buf_size+offset;
        p_pack_buf = fecBuf.sent_buf;
    }
    return p_pack_buf;
}

/**
*把包内容加上fec头，以便支持动态k,n;
*/
const char* pack_fec_head(FecCodecBuf& fecBuf, const FecCodecHead& fec_head, const char* p_buf, int buf_size, int& packed_size)
{
    const char* p_pack_buf = NULL;
    if (p_buf == NULL || buf_size<0)
    {
        packed_size = -1;
        return NULL;
    }
    if (buf_size > fecBuf.enc_pkt_size)
    {
        packed_size = -1;
        return NULL;
    }
    
	if (fecBuf.sent_buf != NULL)
	{
        IUINT16 codec_ikn = 0x0000;
        IUINT16 codec_n = IUINT16(fec_head.codec_n);
        IUINT16 codec_k = IUINT16(fec_head.codec_k);
        codec_k = codec_k<<4;
        IUINT16  ik     = IUINT16(fec_head.ik);
        ik  = ik<<8;

        codec_ikn = codec_ikn|codec_n;
        codec_ikn = codec_ikn|codec_k;
        codec_ikn = codec_ikn|ik;

		int offset = 0;
        char* pEncodeBuf = fecBuf.sent_buf;
        memset(fecBuf.sent_buf, 0, fecBuf.enc_pkt_size); //保守起见；
		unsigned char tagPktHead = tagFecPktHead;
		if (fecBuf.is_send_checksum)
		{
			tagPktHead = tagFecPktHeadCheksum;
		}
        pEncodeBuf = iencode8u(pEncodeBuf, tagPktHead);
        offset += 1;
        pEncodeBuf = iencode32u_lsb(pEncodeBuf, fec_head.sent_pkt_index);
        offset += 4;
        pEncodeBuf = iencode32u_lsb(pEncodeBuf, fec_head.src_pkt_index );
         offset += 4;
        pEncodeBuf = iencode16u_lsb(pEncodeBuf, codec_ikn );
        offset += 2;

        if (fecBuf.is_send_checksum)
        {
            pEncodeBuf = add_checksum(pEncodeBuf, p_buf, buf_size);
            offset += 2;
        }
        memcpy(pEncodeBuf, p_buf, buf_size);
        packed_size = buf_size+offset;
		p_pack_buf = fecBuf.sent_buf;
	}
	return p_pack_buf;
}
/**
*p_buf: IN 包；
*fec_head OUT
*return: 去掉包头后的paket；
*/
const char* unpack_fec_head(FecCodecBuf& fecBuf,  FecCodecHead& fec_head, const char* p_buf, int buf_size, int& unpacked_size)
{
    if (p_buf == NULL || buf_size<0)
    {
        unpacked_size = -1;
        return NULL;
    }
	
	if (fecBuf.dec_buf == NULL)
	{
		unpacked_size = 0;
		return NULL;
	}
    if (buf_size > fecBuf.dec_pkt_size)
    {
        realloc_fec_buf(fecBuf, fecBuf.enc_pkt_size, fecBuf.enc_kmax, buf_size, fecBuf.dec_kmax);
    }
    if (buf_size > fecBuf.dec_pkt_size)
    {
        unpacked_size = 0;
        return NULL;
    }

    const char* p_unpack_buf = NULL;
    memset(fecBuf.dec_buf, 0, fecBuf.dec_pkt_size);
    memcpy(fecBuf.dec_buf, p_buf, buf_size);

	//parse the packet index;
	unsigned int offset = 0;
	IUINT8 fectag = 0;
    const char* pDecodeBuf = fecBuf.dec_buf;

    pDecodeBuf = idecode8u(pDecodeBuf, &fectag);
    if ( (fectag != tagFecPktHead && fectag != tagFecPktHeadCheksum) || buf_size < 11)
    {
        p_unpack_buf = pDecodeBuf;
        unpacked_size = buf_size-1; //tag: 1 bytes;
        return p_unpack_buf;
    }

	fecBuf.is_checksum = (fectag == tagFecPktHeadCheksum);

    offset += 1;
    IUINT32  i_recv_pkt = 0;
    IUINT32 curSentSrcPkt = (IUINT32)-1;
    IUINT16 codec_ikn = 0x0000;
    pDecodeBuf = idecode32u_lsb(pDecodeBuf, &i_recv_pkt);
     offset += 4;
    pDecodeBuf = idecode32u_lsb(pDecodeBuf, &curSentSrcPkt);
    offset += 4;
    
    pDecodeBuf = idecode16u_lsb(pDecodeBuf, &codec_ikn );
    offset += 2;
    IUINT16 cur_n  = codec_ikn&0x000F; codec_ikn = codec_ikn>>4;
    IUINT16 cur_k  = codec_ikn&0x000F; codec_ikn = codec_ikn>>4;
    IUINT16 cur_ni = codec_ikn&0x000F; 
   
    memset(fecBuf.dec_check_pkt, 0 ,fecBuf.dec_pkt_size); //似乎没必要，保守起见吧。
    memcpy(fecBuf.dec_check_pkt, fecBuf.dec_buf + offset, buf_size-offset);
    unpacked_size    = buf_size - offset;
    p_unpack_buf     = fecBuf.dec_check_pkt;
    if (fecBuf.is_checksum)
    {
        p_unpack_buf = rm_checksum(p_unpack_buf, unpacked_size);
		if (p_unpack_buf != NULL)
		{
			unpacked_size -= 2; //2 is checksum size;
		}
    }

    //解析包内容
    fec_head.codec_k = (unsigned char) cur_k ;
	fec_head.codec_n = (unsigned char)  cur_n ;
	fec_head.ik      = (unsigned char)  cur_ni;
	fec_head.sent_pkt_index = i_recv_pkt;
	fec_head.src_pkt_index = curSentSrcPkt;
	return p_unpack_buf;
}



void init_fec_buf(FecCodecBuf& fecBuf, int _max_pkt_size, int _kmax)
{
	if (_max_pkt_size <=0 || _kmax <= 0)
	{
		return;
	}
	
    fecBuf.enc_pkt_size = 0;
    fecBuf.enc_kmax = 0;
    fecBuf.fec_en_buf = NULL;
    fecBuf.sent_buf = NULL;
    fecBuf.en_check_pkt = NULL;
    fecBuf.dec_pkt_size = 0;
    fecBuf.dec_kmax = 0;
    fecBuf.fecDecoderBuf = NULL;
    fecBuf.fecDecoderIndices = NULL;
    fecBuf.dec_buf = NULL;
    fecBuf.dec_check_pkt = NULL;
    int packedSize = getPackedPktSize(_max_pkt_size);
    realloc_fec_buf(fecBuf, packedSize, _kmax, packedSize, _kmax);
  	return;
}
void release_fec_buf(FecCodecBuf& fecBuf)
{
    release_fec_enc_buf(fecBuf);
    release_fec_dec_buf(fecBuf);
}

void release_fec_enc_buf(FecCodecBuf& fecBuf)
{
	if (fecBuf.fec_en_buf != NULL)
	{
		for (int i=0; i<fecBuf.enc_kmax; i++)
		{
			char* p = fecBuf.fec_en_buf[i];
			if (p != NULL)
			{
				free(p);
                fecBuf.fec_en_buf[i] = NULL;
			}
		}
		free(fecBuf.fec_en_buf);
		fecBuf.fec_en_buf = NULL;
	}
	if (fecBuf.sent_buf != NULL)
	{
		free(fecBuf.sent_buf);
		fecBuf.sent_buf = NULL;
	}
	if (fecBuf.en_check_pkt != NULL)
	{
		free(fecBuf.en_check_pkt);
		fecBuf.en_check_pkt = NULL;
	}
	return;
}

void release_fec_dec_buf(FecCodecBuf& fecBuf)
{
	if (fecBuf.fecDecoderBuf)
	{
		for (int i=0; i<fecBuf.dec_kmax; i++)
		{
			if (fecBuf.fecDecoderBuf[i] != NULL)
			{
				free(fecBuf.fecDecoderBuf[i]);
                fecBuf.fecDecoderBuf[i] = NULL;
			}
		}
		free(fecBuf.fecDecoderBuf);
		fecBuf.fecDecoderBuf = NULL;
	}
	if (fecBuf.fecDecoderIndices)
	{
		free(fecBuf.fecDecoderIndices);
		fecBuf.fecDecoderIndices = NULL;
	}
	if (fecBuf.dec_buf != NULL)
	{
		free(fecBuf.dec_buf);
		fecBuf.dec_buf = NULL;
	}
	if (fecBuf.dec_check_pkt != NULL)
	{
		free(fecBuf.dec_check_pkt);
		fecBuf.dec_check_pkt = NULL;
	}
	return;
}


void realloc_fec_buf(FecCodecBuf& fecBuf, int enc_pkt_size, int enc_kmax, int dec_pkt_size, int dec_kmax)
{
    if (enc_pkt_size <=0 || enc_kmax <= 0 || dec_pkt_size<=0 || dec_kmax <= 0)
	{
		return;
	}
    int old_enc_kmax     = fecBuf.enc_kmax;
    int old_dec_kmax     = fecBuf.dec_kmax;

    if (enc_kmax > old_enc_kmax)
    {
        if (fecBuf.fec_en_buf != NULL)
        {
            fecBuf.fec_en_buf = (char **)realloc(fecBuf.fec_en_buf, enc_kmax*sizeof(char*));
        }
        else
        {
            fecBuf.fec_en_buf = (char **)calloc(enc_kmax, sizeof(char*));
        }
        
        for( int i = old_enc_kmax; i<enc_kmax; i++)
        {
            fecBuf.fec_en_buf[i] = (char *)calloc(enc_pkt_size, sizeof(char)); //new allocated;
        }
        fecBuf.enc_kmax = enc_kmax;
    }
    if (dec_kmax > old_dec_kmax)
    {
        if (fecBuf.fecDecoderBuf != NULL)
        {
            fecBuf.fecDecoderBuf = (char **)realloc(fecBuf.fecDecoderBuf, dec_kmax*sizeof(char*));
        }
        else
        {
            fecBuf.fecDecoderBuf = (char **)calloc(dec_kmax, sizeof(char*));
        }
        
        if (fecBuf.fecDecoderIndices != NULL)
        {
            fecBuf.fecDecoderIndices = (int*)realloc(fecBuf.fecDecoderIndices, dec_kmax*sizeof(int));
        }
        else
        {
            fecBuf.fecDecoderIndices = (int*)calloc(dec_kmax, sizeof(int));
        }
        
        for( int i = old_dec_kmax; i<dec_kmax; i++)
        {
            fecBuf.fecDecoderBuf[i] = (char *)calloc(dec_pkt_size, sizeof(char)); //new allocated;
            fecBuf.fecDecoderIndices[i] = -1;
        }
        fecBuf.dec_kmax = dec_kmax;
    }
    
    if (enc_pkt_size > fecBuf.enc_pkt_size)
    {
        int packedsize =(enc_pkt_size);
        for (int i =0; i<fecBuf.enc_kmax; i++)
        {
            if (fecBuf.fec_en_buf[i] == NULL)
            {
                fecBuf.fec_en_buf[i] = (char*)calloc(packedsize, sizeof(char));
            }
            else
            {
               fecBuf.fec_en_buf[i] = (char*)realloc(fecBuf.fec_en_buf[i], packedsize*sizeof(char));
            }
            
        }
        if (fecBuf.sent_buf == NULL)
        {
            fecBuf.sent_buf =  (char*)calloc(packedsize, sizeof(char));
        }
        else
        {
             fecBuf.sent_buf =  (char*)realloc(fecBuf.sent_buf, packedsize*sizeof(char));
        }
       
        if (fecBuf.en_check_pkt == NULL)
        {
            fecBuf.en_check_pkt =  (char*)calloc(packedsize, sizeof(char));
        }
        else
        {
            fecBuf.en_check_pkt =  (char*)realloc(fecBuf.en_check_pkt, packedsize*sizeof(char));
        }
        
        fecBuf.enc_pkt_size = packedsize;
    }

    if (dec_pkt_size > fecBuf.dec_pkt_size)
    {
        int packedsize = (dec_pkt_size);
        for (int i =0; i<fecBuf.dec_kmax; i++)
        {
            if (fecBuf.fecDecoderBuf[i] == NULL)
            {
                fecBuf.fecDecoderBuf[i] = (char*)calloc(packedsize, sizeof(char));
            }
            else
            {
                fecBuf.fecDecoderBuf[i] = (char*)realloc(fecBuf.fecDecoderBuf[i], packedsize*sizeof(char));
            }
        }
        if (fecBuf.dec_buf == NULL)
        {
            fecBuf.dec_buf =  (char*)calloc(packedsize, sizeof(char));
        }
        else
        {
            fecBuf.dec_buf =  (char*)realloc(fecBuf.dec_buf, packedsize*sizeof(char));
        }
        if (fecBuf.dec_check_pkt == NULL)
        {
            fecBuf.dec_check_pkt =  (char*)calloc(packedsize, sizeof(char));
        }
        else
        {
            fecBuf.dec_check_pkt =  (char*)realloc(fecBuf.dec_check_pkt, packedsize*sizeof(char));
        }
        
        fecBuf.dec_pkt_size = packedsize;
    }
}


