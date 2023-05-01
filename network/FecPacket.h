#ifndef _AUDIOMAIN_NETWORK_FEC_PACKET_H_
#define _AUDIOMAIN_NETWORK_FEC_PACKET_H_


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

class FecPacket
{
public:
	long  iPacket;
	char* FecBuf;
	int   BufSize;
	bool  bValid;
	int   MaxBufSize;
	bool  bSourcePkt;
	long  i_source_pkt; //对应的原始包号码
    bool  bUsed;

public:
	FecPacket(int size)
	{
		FecBuf = 0;
		bValid = false;
		MaxBufSize = size;
		FecBuf = NULL;
		BufSize = 0;
		iPacket = -1;
		bSourcePkt = true;
        bUsed = false;
		i_source_pkt = -1;
	}

	FecPacket(const FecPacket& right)
	{
		iPacket = right.iPacket;
		FecBuf = right.FecBuf;
		MaxBufSize = right.MaxBufSize;
		BufSize = right.BufSize;
		bValid = right.IsValid();
		bSourcePkt = right.bSourcePkt;
		i_source_pkt = right.i_source_pkt;
        bUsed = right.bUsed;
		return;
	}

	FecPacket& operator= (const FecPacket& right)
	{
		iPacket = right.iPacket;
        int oldMaxBufSize = MaxBufSize;
		if (MaxBufSize != right.GetMaxSize())
		{
			MaxBufSize = right.GetMaxSize();
		}
		if (FecBuf == NULL)
		{
			FecBuf = (char*)calloc(MaxBufSize, sizeof(char));
			memset(FecBuf, 0, MaxBufSize);
		}
        else
        {
            if (oldMaxBufSize != MaxBufSize)
            {
                FecBuf = (char*)realloc(FecBuf, MaxBufSize);
            }
        }
		memcpy(FecBuf, right.FecBuf, right.BufSize);
		BufSize = right.BufSize;
		bValid = right.IsValid();
		bSourcePkt = right.bSourcePkt;
		i_source_pkt = right.i_source_pkt;
        bUsed = right.bUsed;
		return *this;
	}

	void SetPacket( const char* pBuf, int size)
	{
		if (pBuf == NULL)
		{
			return;
		}
        if (FecBuf == NULL)
        {
            FecBuf = (char*)calloc(MaxBufSize, sizeof(char));
            memset(FecBuf,0,MaxBufSize);
        }
        if (size > MaxBufSize)
        {
            FecBuf = (char*)realloc(FecBuf, size);
            MaxBufSize = size;
        }
        memset(FecBuf,0, MaxBufSize);
		memcpy(FecBuf, pBuf, size);
		BufSize = size;
		bValid = true;
        bUsed = false;
	}
	void Reset(int max_size)
	{
		iPacket = -1;
		BufSize = 0;
		bValid = 0;
        if (FecBuf == NULL)
        {
            FecBuf = (char*)calloc(max_size, sizeof(char));
            memset(FecBuf,0,max_size);
        }

		if (FecBuf)
		{
			if (MaxBufSize != max_size)
			{
				FecBuf = (char*)realloc(FecBuf, max_size);
			}
			memset(FecBuf,0,max_size);
			BufSize = 0;
		}
		MaxBufSize = max_size;
		bValid = false;
        bUsed = false;
	}
	bool IsValid() const
	{
		return FecBuf != NULL && bValid;
	}
	int GetMaxSize() const {return MaxBufSize; }
};


#endif


