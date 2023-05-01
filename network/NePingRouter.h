#ifndef _AUDIO_ENGINE_NE_PING_ROUTER_
#define _AUDIO_ENGINE_NE_PING_ROUTER_

#include <map>
#include <string>
#include <vector>
#include "../system/system.h"

#ifdef _WIN32
	#include <WinSock2.h>
	#include <Ipexport.h>
#endif


//
//此处是一个非常简单的表，仅中转一次，所以对使用者要求比较高；
//路由算法也是非常粗略的。
//
class NeHostRouteTable
{
private:
	//<e(p0->p1), weight>;
	std::map< std::pair<unsigned long, unsigned long> , std::vector<unsigned int> > m_mapEdgesWeight;
	int m_nTime;
protected:
	//根据weight历史，计算边的weight
	unsigned int CmpWeight( const std::vector<unsigned int> & vecWeight);

public:

	NeHostRouteTable();
	~NeHostRouteTable();

	/**
	*在map中维护weight列表时，每次更新weight,保留的weight的个数
	*/
	void SetPingTimes(int right ) { m_nTime = right; }

	//设置某边的权重；
	void SetEdgeWeight( unsigned long nSIP, unsigned long nEIP, unsigned int nWeight);
	//寻找从snIP到nEIP的一边， 存在，就返回weight，否则返回-1;
	unsigned int GetEdgeWeight(  unsigned long nSIP, unsigned long nEIP);
	//寻找从snIP到nEIP的一条路由， out: nNextIP; 返回weight;
	unsigned int GetRoute(unsigned long nSIP, unsigned long nEIP, unsigned long& nNextIP);
};



class NePinger 
{
private:
	//ip-> < SN, time used>
	//std::map< unsigned long,  std::pair<unsigned int, unsigned int> > m_mapHostWeight;

	NeHostRouteTable m_RouteTable;
	
	unsigned int m_nPingTimeout; // time out for ping; in milliion secons;
    int m_nRetryPing;
    int m_nPingSleep;

	std::map<std::string, unsigned long> m_vecMidHostIP; //midle transfer host ip;
	std::map<std::string, unsigned int> m_vecMidHost;
	std::pair<std::string, unsigned long> m_szDestHostIP; //目的主机IP,sn；

    std::pair<std::string,int> m_szCGIHost;
	
	iPosixThread *m_hThread;
	bool  m_bStopWork;

	System::ReadWriteLock m_Lock;
	unsigned int m_nDestPort;
	unsigned int m_nCuteID;
	
	bool m_bRuning;

	std::pair<std::string,unsigned int> m_TransferHost;


protected:
	void processRecv(unsigned long _status, unsigned long _RTT, const char *pSrcIP, unsigned int nSN);
	
	//send ping and recv the replied information;
	bool wndSendPing(const char* pDestIP, unsigned short nID, unsigned int sn, unsigned int nTimeout);

	//添加和删除中间转发节点；
	void addTransferHostIP( const char* pDestIP );
	void removeTransferHostIP( const char* pDestIP );

	int openUrl(const char* pszAudioIP, unsigned int nAudioPort, unsigned int nCuteID, 
		std::string& content, long& resp);


	void reqRttCGI();

	static int WinThreadProc( void* lpParam );

	bool isRunning();
	void setRuning(bool right);
	const char* getDestHostIP();
	unsigned int getDestPort();
	int getPingTimes();
    unsigned int getPingTimeout();
    unsigned int getPingSleep();
	//
	void setTransferHost(const std::string& ip, unsigned int port);
	//获取到目的地址的路由，并返回中间结点IP和RTT
	unsigned int getRouteRTT(std::string& szTransferIP, bool & bTransfer);

	bool isWorkStop();

public:
	NePinger(void);
	~NePinger(void);

	void SetDestHostIP( const char * pDestIP );
	void SetDestPort(unsigned int right );
	void SetCuteID( unsigned int right );
    unsigned int GetCuteID();

    //http://192.168.35.247:8866/qnet/get_node_list?key=
    void SetCGIHost(const char* pszIP, int port);

    bool IsReady();
	void SetPingTimes(int right);
	void SetPingTimeout( unsigned int nTimeout );
    void SetPingSleep( int nMS);

	const char* GetTransferHostIP();
	unsigned int GetTransferHostPort();
    void ResetTransferHost();

	void Run();

	bool Start();

	void PostTerminate();
};


#endif

