#ifndef  _AUDIO_MAIN_NET_ZFEC_TRANSMISSION_H
#define  _AUDIO_MAIN_NET_ZFEC_TRANSMISSION_H

#include "ProtocolBasic.h"
#include "NetFecCodec.h"

NAMESPACE_BEGIN(QuickNet)

class FecTransmission : public Transmission
{
public:
    FecTransmission() { m_bZfec = true;}
    virtual ~FecTransmission();

    void Init(int maxSizePkt, int zfecBufItemNum, int maxk, int k, int n, bool enabled, bool is_sorted);

    // 输入下层协议的 packet，由外层调用
    virtual void PacketInput(ProtocolPacket *packet);

    // 发送上层数据
    virtual void Send(ProtocolPacket *packet) ;

    // 更新状态
    virtual void Update(IUINT32 current) ;

    // 返回 OVERHEAD
    virtual int GetOverhead() const;

    // 设置
    virtual int Option(int option, int value);

    // 取得状态
    virtual int GetStatus(int option) const;

	static int zfecPackCallback(void *p, const char* buf, unsigned int sizepkt);

	static int zfecUnpackCallback(void *p, const char* buf, unsigned int sizepkt, IUINT32 i_src_pkt);

private:
     NetFecCodecLayer zfec;
     bool m_bZfec;
   
};


Transmission* CreateFecTransmission();





NAMESPACE_END(QuickNet)


#endif


