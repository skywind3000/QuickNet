#include "FecTransmission.h"
#include "ProtocolBasic.h"

NAMESPACE_BEGIN(QuickNet)

void FecTransmission::PacketInput(ProtocolPacket *packet)
{
    if (packet == NULL)
    {
        return;
    }
    if (m_bZfec)
    {
        zfec_unpack_input(zfec, this,  packet->data(), packet->size());
        delete packet;
    }
    else
    {
        Deliver(packet);
        
    }
   
}

void FecTransmission::Send(ProtocolPacket *packet)
{
    if (packet == NULL)
    {
        return;
    }
    if (m_bZfec)
    {
        zfec_pack_input(zfec,this, packet->data(), packet->size(), -1);
        delete packet;
    }
    else
    {
        Output(packet);

    }
}

void FecTransmission::Update(IUINT32 current)
{
    //nothing to do.

}

int FecTransmission::GetOverhead() const
{
    return 11;//FEC head : 11 Bytes;
}

// 设置
int FecTransmission::Option(int option, int value)
{
    int iRet = -1;
    switch(option)
    {
    case QUICKNET_OPT_FEC_MAXBUFSIZE:
        {
            break;
        }
    case QUICKNET_OPT_FEC_BUFITEM_NUM:
        {
            break;
        }
    case QUICKNET_OPT_FEC_MAXK:
        {
            break;
        }
    case QUICKNET_OPT_FEC_ENABLED:
        {
            enable_zfec(zfec, value != 0);
            iRet = 0;
            break;
        }
    case QUICKNET_OPT_FEC_SORTED:
        {
            enable_sorted_zfec(zfec, value != 0);
            iRet = 0;
            break;
        }
    case QUICKNET_OPT_FEC_LOST_RATE:
        {
            zfec.lost_rate = (float)(abs(value) )/100.0f;
            iRet = 0;
            break;
        }
    case QUICKNET_OPT_FEC_DYNKN:
        {
            enable_zfec_dynkn(zfec, value != 0);
            iRet = 0;
            break;
        }
    case QUICKNET_OPT_FEC_STATIC_K:
        {
            if (value <2 && value >= 7)
            {
                iRet = -1;
                break;
            }

            int curK = -1;
            int curN = -1;
            get_zfec_kn(zfec, curK,curN);
            if (curK != -1 && curN != -1)
            {
                if (value <= curN && (float)(value) >= (float)(curN)/2.0)
                {
                    set_zfec_kn(zfec, value, curN, true);
                }
                else
                {
                    set_zfec_kn(zfec, value, value+2, true);
                }
            }
            else
            {
                set_zfec_kn(zfec, value, value+2, true);
            }
            iRet = 0;
            break;
        }
    case QUICKNET_OPT_FEC_STATIC_N:
        {
            if (value <4 && value >= 10)
            {
                iRet = -1;
                break;
            }
            int n = value;
            int k = value%2 ==0? value/2 : value/2+1;

            int curK = -1;
            int curN = -1;
            get_zfec_kn(zfec, curK,curN);
            if (curK != -1 && curN != -1)
            {
                if (value > curK && (float)(value) <= (float)(curK)*2.0)
                {
                    set_zfec_kn(zfec, curK, n, true);
                }
                else
                {
                    set_zfec_kn(zfec, k, n, true);
                }
            }
            else
            {
                int k = value%2 ==0? value/2 : value/2+1;
                set_zfec_kn(zfec, k, value, true);
            }
            iRet = 0;
            break;
        }
    default:
        {

        }
    }
    return iRet;
}

// 取得状态
int FecTransmission::GetStatus(int option) const
{
    int iRet = -1;
    switch(option)
    {
    case QUICKNET_OPT_FEC_MAXBUFSIZE:
        {
            break;
        }
    case QUICKNET_OPT_FEC_BUFITEM_NUM:
        {
            break;
        }
    case QUICKNET_OPT_FEC_MAXK:
        {
            break;
        }
    case QUICKNET_OPT_FEC_ENABLED:
        {
            iRet = is_zfec_enabled(zfec);
            break;
        }
    case QUICKNET_OPT_FEC_SORTED:
        {
            iRet = is_sorted_zfec(zfec);
            break;
        }
    case QUICKNET_OPT_FEC_LOST_RATE:
        {
            iRet = int(zfec.lost_rate*100);
            break;
        }
    case QUICKNET_OPT_FEC_DYNKN:
        {
            iRet = is_zfec_dynkn(zfec);
            iRet = int(iRet);
            break;
        }
    case QUICKNET_OPT_FEC_STATIC_K:
        {
            int curK = -1;
            int curN = -1;
            get_zfec_kn(zfec, curK,curN);
            iRet = curK;
            break;
        }
    case QUICKNET_OPT_FEC_STATIC_N:
        {
            int curK = -1;
            int curN = -1;
            get_zfec_kn(zfec, curK,curN);
            iRet = curN;
            break;
        }
    case QUICKNET_OPT_FEC_RECV_PKT:
        {
            iRet = zfec.fec_src_count;
            break;
        }
    case QUICKNET_OPT_FEC_FEC_RESTORE_PKT:
        {
            iRet = zfec.fec_restore_count;
            break;
        }

    default:
        {

        }
    }
    return iRet;
}


void FecTransmission::Init(int maxSizePkt, int zfecBufItemNum, int maxk, int k, int n, bool enabled, bool is_sorted)
{
    init_zfec_layer(zfec, maxSizePkt, zfecBufItemNum, maxk);
	zfec.trace = &Trace::Global;
	//创建候选k,n列表；
	/**
	*0.5，0.6，0.625，0.67，0.75，0.8，0.83，0.875
	*/
	int karray[8] = {2,3,5,4,3,4,5,7};
	int narray[8] = {4,5,8,6,4,5,6,8};
	for(int i = 0; i<8; i++)
	{
		set_zfec_kn(zfec, karray[i], narray[i],true);
	}
	set_zfec_kn(zfec, k, n,true);
	enable_zfec(zfec, enabled);
	enable_sorted_zfec(zfec, is_sorted);

	zfec.PackOutput = &(FecTransmission::zfecPackCallback);
	zfec.UnpackOutput = &(FecTransmission::zfecUnpackCallback);
}

FecTransmission::~FecTransmission() 
{
    release_z_fec_layer(zfec);
}


int FecTransmission::zfecPackCallback(void *p, const char* buf, unsigned int sizepkt)
{
    int nRet = -1;

    FecTransmission *param = (FecTransmission*)p;
    if (param == NULL )
    {
        return nRet;
    }

    ProtocolPacket *packet = new ProtocolPacket(sizepkt);
    packet->push_tail(buf, sizepkt);
    param->Output(packet);
    //把他们送给信道channel;
    return nRet;
}


int FecTransmission::zfecUnpackCallback(void *p, const char* buf, unsigned int sizepkt,  IUINT32 i_src_pkt)
{
    int nRet = -1;
    FecTransmission *param = (FecTransmission*)p;
    if (param == NULL )
    {
        return nRet;
    }
    ProtocolPacket *packet = new ProtocolPacket(sizepkt);
    packet->push_tail(buf, sizepkt);
    param->Deliver(packet);

    return nRet;
}


Transmission* CreateFecTransmission()
{
    FecTransmission* pFecTransmision = new FecTransmission();
    if (pFecTransmision != NULL)
    {
        pFecTransmision->Init(2048, 48, 10, 4, 5, true,false);
    }
    return pFecTransmision;
}


NAMESPACE_END(QuickNet)



