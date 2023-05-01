#include "../network/ProtocolBasic.h"

#include "NePingRouter.h"
#include "../system/imemdata.h"

NeHostRouteTable::NeHostRouteTable()
{
	m_nTime = 10;
}

NeHostRouteTable::~NeHostRouteTable()
{

}

//设置某边的权重；
void NeHostRouteTable::SetEdgeWeight( unsigned long nSIP, unsigned long nEIP, unsigned int nWeight)
{
	std::pair<unsigned long, unsigned long> e(nSIP, nEIP);
	std::vector<unsigned int> & vecWeight = m_mapEdgesWeight[e];
	vecWeight.push_back(nWeight);
	if ((int)vecWeight.size()>m_nTime)
	{
		vecWeight.erase( vecWeight.begin(), vecWeight.begin()+ vecWeight.size()-m_nTime );
	}
}


unsigned int NeHostRouteTable::CmpWeight( const std::vector<unsigned int> & vecWeight)
{
	//一下采用简单计算；
	unsigned int nRet = (unsigned int)(-1);
	int n = (int)vecWeight.size();
	int nt = 0;
	for (std::vector<unsigned int>::const_iterator cit = vecWeight.begin();
		cit != vecWeight.end(); cit++)
	{
		if (*cit == (unsigned int)(-1))
		{
			nt++;
		}
	}
	if (float(nt) >= float(n)/2.0f)
	{
		return nRet; //权重为-1的个数已经超过一半了。
	}
	nt = 0;
	unsigned int nWeight = 0;
	for (std::vector<unsigned int>::const_iterator cit = vecWeight.begin();
		cit != vecWeight.end(); cit++)
	{
		if (*cit != (unsigned int)(-1))
		{ 
			nt++; nWeight += *cit;
		}
	}
    if (nt>0)
    {
        nRet = nWeight/nt;
    }
	return nRet;
}

//寻找从snIP到nEIP的一边， 存在，就返回weight，否则返回-1;
unsigned int NeHostRouteTable::GetEdgeWeight(  unsigned long nSIP, unsigned long nEIP)
{
	unsigned int nRet = (unsigned int)(-1);
	std::pair<unsigned long, unsigned long> e(nSIP, nEIP);
	std::map< std::pair<unsigned long, unsigned long> , std::vector<unsigned int> >::iterator it = m_mapEdgesWeight.end();
	it = m_mapEdgesWeight.find(e);
	if (it != m_mapEdgesWeight.end())
	{
		nRet = CmpWeight(it->second);
	}
	return nRet;
}

//寻找从snIP到nEIP的一条路由， out: nNextIP; 返回weight;
unsigned int NeHostRouteTable::GetRoute(unsigned long nSIP, unsigned long nEIP, unsigned long& nNextIP)
{
	std::map< std::pair<unsigned long, unsigned long> , std::vector<unsigned int> >::iterator it = m_mapEdgesWeight.end();
	std::map< std::pair<unsigned long, unsigned long> , std::vector<unsigned int> >::iterator eit = m_mapEdgesWeight.end();

	unsigned long nOutIP = nEIP;
	unsigned int nRetWeight = (unsigned int)(-1);
	eit =  m_mapEdgesWeight.find( std::pair<unsigned long, unsigned long>(nSIP, nEIP) );
	if (eit == m_mapEdgesWeight.end())
	{
		printf("[ERROR] Failed to locate one read route. The procedure just calc an candidate route\n");
		nRetWeight = (unsigned int)(-1);
	}
	else
	{
		nRetWeight = CmpWeight( eit->second );
	}
	

	for (it = m_mapEdgesWeight.begin(); it != m_mapEdgesWeight.end(); it++)
	{
		if (it->first.first != nSIP)
		{
			continue;
		}
		unsigned int nWeight0 = CmpWeight(it->second);
		if (nWeight0 == (unsigned int)(-1))
		{
			continue;
		}
		unsigned long nMidIP = it->first.second;
		eit = m_mapEdgesWeight.end();
		eit = m_mapEdgesWeight.find( std::pair<unsigned long, unsigned long>(nMidIP, nEIP) );
		if (eit != m_mapEdgesWeight.end())
		{
			unsigned int nWeight =  CmpWeight(eit->second);
			if (nWeight != (unsigned int)(-1) && nRetWeight > nWeight0 + nWeight)
			{
				nRetWeight = nWeight0 + nWeight;
				nOutIP = nMidIP;
			}
		}
	}
	nNextIP = nOutIP;
	return nRetWeight;
}

int NePinger::WinThreadProc( void* lpParam )
{
	NePinger *pThread = (NePinger*)lpParam;
	if (pThread == NULL)
		return 0;
	pThread->Run();

	return 0;
}

NePinger::NePinger(void) 
{
 	m_nPingTimeout = 1000;
	m_nRetryPing = 10;
    m_nPingSleep = 1;
	m_nCuteID = 0;
	m_nDestPort = 0;
	m_bRuning = false;
	m_TransferHost.first = "";
	m_TransferHost.second = 0;
    //114.113.200.143:8866
    m_szCGIHost.first =  "114.113.200.143";
    m_szCGIHost.second = 8866;
    m_hThread = NULL;
}

NePinger::~NePinger(void)
{
    if (m_hThread != NULL)
    {
        PostTerminate();
        int hr = iposix_thread_join(m_hThread, 0xffffffff);
        if (hr != 0)
        {
            QuickNet::Trace::Global.out(TRACE_ERROR, "[ROUTER]  Fail to Stop Router!.[stime]=%u", iclock());
            return;
        }
        iposix_thread_delete(m_hThread);
        QuickNet::Trace::Global.out(TRACE_ERROR, "[ROUTER]  Router stop!.[stime]=%u", iclock());
    }
}

void NePinger::SetDestHostIP( const char * pDestIP ) 
{
	if (pDestIP == NULL)
	{
		return;
	}

	m_Lock.write_lock();
	m_szDestHostIP.first = pDestIP;
	m_szDestHostIP.second = 0;
	m_Lock.write_unlock();
}

void NePinger::SetDestPort(unsigned int right ) 
{ 
	m_Lock.write_lock();
	m_nDestPort = right; 
	m_Lock.write_unlock();
}
void NePinger::SetCuteID( unsigned int right ) 
{ 
	m_Lock.write_lock();
	m_nCuteID = right; 
	m_Lock.write_unlock();
}

void NePinger::SetPingTimes(int right)
{
	m_Lock.write_lock();
	m_nRetryPing = right; 
	m_Lock.write_unlock();
}

void NePinger::SetPingSleep( int nMS)
{
    m_Lock.write_lock();
    m_nPingSleep = nMS; 
    m_Lock.write_unlock();
}
  

void NePinger::SetPingTimeout( unsigned int nTimeout ) 
{ 
    m_Lock.write_lock();
    m_nPingTimeout = nTimeout; 
    m_Lock.write_unlock();
}

unsigned int NePinger::getPingTimeout()
{
    unsigned int nRet = 0;
    m_Lock.read_lock();
    nRet  = m_nPingTimeout; 
    m_Lock.read_unlock();
    return nRet;
}

unsigned  int NePinger::getPingSleep()
{
    unsigned int nRet = 0;
    m_Lock.read_lock();
    nRet  = m_nPingSleep; 
    m_Lock.read_unlock();
    return nRet;
}

int NePinger::getPingTimes()
{
	int nRet = 0;
	m_Lock.read_lock();
	nRet  = m_nRetryPing; 
	m_Lock.read_unlock();
	return nRet;
}

bool NePinger::IsReady()
{
    bool bRet = false;
    m_Lock.read_lock();
    bRet = m_nCuteID>0 && m_nDestPort>0 && m_szDestHostIP.first.size()>0;
    m_Lock.read_unlock();
    return bRet;
}

unsigned int NePinger::GetCuteID()
{
	unsigned int nRet = 0;
	m_Lock.read_lock();
	nRet  = m_nCuteID; 
	m_Lock.read_unlock();
	return nRet;
}

const char* NePinger::getDestHostIP()
{
	const char* pRet = 0;
	m_Lock.read_lock();
	pRet = m_szDestHostIP.first.c_str();
	m_Lock.read_unlock();
	return pRet;
}

unsigned int NePinger::getDestPort()
{
	unsigned int nRet = 0;
	m_Lock.read_lock();
	nRet  = m_nDestPort; 
	m_Lock.read_unlock();
	return nRet;
}

bool NePinger::isRunning()
{
	bool bRet = false;
	m_Lock.read_lock();
	bRet  = m_bRuning; 
	m_Lock.read_unlock();
	return bRet;
}
void NePinger::setRuning(bool right)
{
	m_Lock.write_lock();
	m_bRuning = right; 
	m_Lock.write_unlock();
}


void NePinger::setTransferHost(const std::string& ip, unsigned int port)
{
	m_Lock.write_lock();
	m_TransferHost.first = ip;
	m_TransferHost.second = port;
	m_Lock.write_unlock();
}

void  NePinger::SetCGIHost(const char* pszIP, int port)
{
    if (pszIP == NULL) return;
    if (isRunning() == false)
    {
        m_szCGIHost.first = pszIP;
        m_szCGIHost.second = port;
    }

}
void NePinger::ResetTransferHost()
{
    setTransferHost("", 0);

}
const char* NePinger::GetTransferHostIP()
{
	const char* pszRet = NULL;
	m_Lock.read_lock();
	pszRet  = m_TransferHost.first.c_str(); 
	m_Lock.read_unlock();
	return pszRet;
}

unsigned int NePinger::GetTransferHostPort()
{
	unsigned int nRet = 0;
	m_Lock.read_lock();
	nRet  = m_TransferHost.second; 
	m_Lock.read_unlock();
	return nRet;

}

void NePinger::addTransferHostIP( const char* pSrcIP )
{
	if (pSrcIP == NULL)
	{
		return;
	}

	std::string strIP(pSrcIP);
	std::map<std::string, unsigned long>::iterator it = m_vecMidHostIP.find( strIP );
	if (it == m_vecMidHostIP.end())
	{
		m_vecMidHostIP.insert( std::pair< std::string, unsigned long> ( strIP, 0) );
	}
}

void NePinger::removeTransferHostIP( const char* pSrcIP )
{
	if (pSrcIP == NULL)
	{
		return;
	}
	std::string strIP(pSrcIP);
	std::map<std::string, unsigned long>::iterator it = m_vecMidHostIP.find( strIP );
	if (it != m_vecMidHostIP.end())
	{
		m_vecMidHostIP.erase( it );
	}
}

unsigned int NePinger::getRouteRTT(std::string& szTransferIP, bool & bTransfer)
{
	unsigned long nSrcIP = 	inet_addr( "127.0.0.1" );
	unsigned long nDestIP = inet_addr( m_szDestHostIP.first.c_str() );
	unsigned long nTransferIP = 0;
	unsigned int nRet = m_RouteTable.GetRoute(nSrcIP, nDestIP, nTransferIP);
	if (nRet != (unsigned int)(-1))
    {
#ifdef _WIN32
        struct in_addr caddr;
        memset((void *)&caddr,0, sizeof(struct in_addr));
        caddr.S_un.S_addr = nTransferIP;
        szTransferIP = inet_ntoa(caddr);
#else
        System::SockAddress sockaddr;
        sockaddr.set_ip(nTransferIP);
        szTransferIP = sockaddr.get_ip_text(NULL);
#endif
        bTransfer = (nTransferIP != nDestIP);
    }
    return nRet;
}



//http://192.168.35.247:8866/qnet/get_node_list?key=value   value=base64("audio_ip,qnet_port,cuteid")

int NePinger::openUrl(const char* pszAudioIP, unsigned int nAudioPort, unsigned int nCuteID, 
					  std::string& content, long& resp)
{
	if (pszAudioIP == NULL)
	{
		return -3;
	}
	char szTextBuf[512] = {0};
	sprintf(szTextBuf,"%s,%u,%u", pszAudioIP, nAudioPort, nCuteID);

	char szDestBuf[2048] = {0};
    ilong iRet = ibase64_encode(szTextBuf, strlen(szTextBuf), szDestBuf);
	char szUrl[4096] = {0};
    //"http://114.113.200.143:8866/qnet/get_node_list?key="; 
	sprintf(szUrl,"http://%s:%d/qnet/get_node_list?key=%s", m_szCGIHost.first.c_str(), m_szCGIHost.second, szDestBuf);

	System::HttpRequest request;
    iRet = -1;
	iRet = System::HttpRequest::wget(szUrl, content,NULL,2000);
	if (iRet >= 0)
	{
		resp = 200;
		return 0;
	}
	resp = 500;
	return -2;
}



void NePinger::reqRttCGI()
{
	std::string strContent;
	long resp;
	std::string szIP = getDestHostIP();
	unsigned int nDestPort = getDestPort();
	unsigned int nCuteID = GetCuteID();
	openUrl(szIP.c_str(), nDestPort, nCuteID, strContent, resp);
	if (resp != 200)
	{
        QuickNet::Trace::Global.out(TRACE_ERROR,
            "[ROUTER] [Failed]=CGI, CGI=%s:%d, Dest=%s:%d, CuteID=%u",
            m_szCGIHost.first.c_str(), m_szCGIHost.second,	szIP.c_str(), nDestPort, nCuteID);
		return;
	}
    int nTransHost = 0;
	std::string::size_type  spos = 0;
	std::string::size_type epos = strContent.find(',', spos);
	while (epos != std::string::npos && spos != epos)
	{
		std::string strItem = strContent.substr(spos, epos-spos);
		if (strItem.size()>0)
		{
			std::string buf;
			buf.assign( strItem.begin(), strItem.begin()+strItem.size() );
			char szIP[64] = {0};
			unsigned int port = 0;
			float weight = 0;
			sscanf(buf.c_str(),"%[^ ] %d %f",szIP, &port, &weight);
			addTransferHostIP(szIP);
			m_vecMidHost[ std::string(szIP) ] = port;
            m_vecMidHostIP[std::string(szIP)] = 0; 
			unsigned int rtt = int(weight);
			unsigned long nSrcIP = inet_addr(szIP);
			unsigned long nDstIP = inet_addr(m_szDestHostIP.first.c_str());
			m_RouteTable.SetEdgeWeight(nSrcIP, nDstIP, rtt);
            nTransHost++;
		}
		if (isWorkStop())
		{
			return;
		}
		spos = epos+1;
		epos = strContent.find(',', spos);
	}
    if (nTransHost==0)
    {     
        QuickNet::Trace::Global.out(TRACE_ERROR,  
            "[ROUTER], [Failed]=CGI Query returned nothing, [CGI-Host]=%s:%d, [DestHost]=%s:%d, [CuteID]=%u",
            m_szCGIHost.first.c_str(), m_szCGIHost.second,	 szIP.c_str(), nDestPort, nCuteID);
		if (isWorkStop())
		{
			return;
		}
            

    }

}

void NePinger::Run()
{
#ifndef _WIN32
    return;
#endif
	setRuning(true);

    int nPingTimes = getPingTimes();
    unsigned int nTimeout = getPingTimeout();
    int nSleepMS = getPingSleep();
	//request RTT server cgi;获取server到转发主机的RTT值。
	m_vecMidHostIP.clear();
	m_vecMidHost.clear();
	setTransferHost("", 0);

	reqRttCGI();
	std::pair<std::string, unsigned long> szDestHostIP;
	szDestHostIP.first = getDestHostIP();
	szDestHostIP.second = 0;
	//ping 主机，加中间结点，把数据放入m_RouteTable;
	std::vector< std::pair<std::string,  unsigned long> > vecHostIP;
	vecHostIP.insert( vecHostIP.end(), m_vecMidHostIP.begin(), m_vecMidHostIP.end() );
	vecHostIP.push_back( szDestHostIP );
	if (isWorkStop())
	{
		return;
	}
    //int nItem = (int)vecHostIP.size();

    m_RouteTable.SetPingTimes( nPingTimes );
	for( int iHost = 0; nPingTimes>0 && iHost < (int)vecHostIP.size()*nPingTimes; iHost++)
	{
		int iCurHost = iHost % int(vecHostIP.size());
		printf("\nping : %s\n", vecHostIP[iCurHost].first.c_str());
		//unsigned long nIP = inet_addr( it->first.c_str() );
#ifdef _WIN32
        unsigned short nID = (unsigned short)::GetCurrentProcessId();
#else
        unsigned short nID = 7788;
#endif
		
		unsigned int nSN = vecHostIP[iCurHost].second;
		wndSendPing( vecHostIP[iCurHost].first.c_str(), nID, nSN, nTimeout);
		if (iCurHost>=0 && iCurHost <(int)m_vecMidHostIP.size())
		{
			m_vecMidHostIP[vecHostIP[iCurHost].first]++;
		}
		else
		{
			m_szDestHostIP.second++;
		}
		if (isWorkStop())
		{
			return;
		}
        if (nSleepMS>0)
        {
            isleep(nSleepMS);
        }
	}
	if (isWorkStop())
	{
		return;
	}
	std::string strTransferIP;
	bool bTransfer = false;
	unsigned int nRtt = getRouteRTT(strTransferIP, bTransfer);
    std::map<std::string, unsigned int>::iterator it = m_vecMidHost.end();
	it = m_vecMidHost.find(strTransferIP);
    if (bTransfer && it != m_vecMidHost.end())
	{
		if (isWorkStop())
		{
			return;
		}
		setTransferHost(strTransferIP.c_str(), it->second);
	}
	setRuning(false);
	if (isWorkStop())
	{
		return;
	}
	unsigned long nSrcIP = inet_addr("127.0.0.1");
	unsigned long nDstIP = inet_addr(m_szDestHostIP.first.c_str());
	unsigned long nTransIP = inet_addr(strTransferIP.c_str());
	QuickNet::Trace::Global.out(TRACE_ERROR,  
		"[ROUTER] RealTransfer=%s, TransHost=%s, DestHost=%s,  SrcToDestRTT=%d, SrcToTransRTT=%d, DestToTransRtt=%d, TranferedRTT=%d",
								  bTransfer?"Y":"N",  strTransferIP.c_str(), m_szDestHostIP.first.c_str(),
								  m_RouteTable.GetEdgeWeight(nSrcIP,nDstIP), m_RouteTable.GetEdgeWeight(nSrcIP,nTransIP), 
								  m_RouteTable.GetEdgeWeight(nTransIP,nDstIP), nRtt);
	QuickNet::Trace::Global.out(TRACE_ERROR, "[ROUTER] Router stop!.[stime]=%u", iclock());
}

bool NePinger::wndSendPing(const char* pDestIP,  unsigned short nID, unsigned int sn, unsigned int nTimeout)
{
	if (pDestIP == NULL)
	{
		return false;
	}

#ifdef _WIN32
    unsigned long nIP = inet_addr( pDestIP );
    HANDLE hIcmpFile;
    typedef HANDLE (WINAPI *_funIcmpCreateFile)(VOID);
    typedef DWORD (WINAPI  *_funIcmpSendEcho)(
        HANDLE                   IcmpHandle,
        IPAddr                   DestinationAddress,
        LPVOID                   RequestData,
        WORD                     RequestSize,
        PIP_OPTION_INFORMATION   RequestOptions,
        LPVOID                   ReplyBuffer,
        DWORD                    ReplySize,
        DWORD                    Timeout
        );

    typedef BOOL (WINAPI   *_funIcmpCloseHandle)(HANDLE);

    static _funIcmpCreateFile funIcmpCreateFile = NULL;
    static _funIcmpSendEcho funIcmpSendEcho = NULL;
    static _funIcmpCloseHandle funIcmpCloseHandle = NULL;
    static HINSTANCE hIcmpDll = NULL;
    if (hIcmpDll == NULL)
    {
        hIcmpDll = ::LoadLibraryA("Iphlpapi.dll"); //Windows Server 2003 and Windows XP
    }
    if (hIcmpDll == NULL)
    {
        hIcmpDll = ::LoadLibraryA("Icmp.dll"); //Windows 2000 Server and Windows 2000 Professional. 
    }
    if (hIcmpDll == NULL)
    {
        return false;
    }
    //IcmpCreateFile
    if (funIcmpCreateFile == NULL)
    {
        funIcmpCreateFile = (_funIcmpCreateFile)::GetProcAddress(hIcmpDll, "IcmpCreateFile");
    }
    if (funIcmpCreateFile == NULL)
    {
        return false;
    }
    //IcmpSendEcho
    if (funIcmpSendEcho == NULL)
    {
        funIcmpSendEcho = (_funIcmpSendEcho)::GetProcAddress(hIcmpDll, "IcmpSendEcho");
    }
    if (funIcmpSendEcho == NULL)
    {
        return false;
    }
    //IcmpCloseHandle
    if (funIcmpCloseHandle == NULL)
    {
        funIcmpCloseHandle = (_funIcmpCloseHandle)::GetProcAddress(hIcmpDll, "IcmpCloseHandle");
    }
    if (funIcmpCloseHandle == NULL)
    {
        return false;
    }

    if ((hIcmpFile = funIcmpCreateFile()) == INVALID_HANDLE_VALUE)
    {
        printf("\tUnable to open file.\n");
        return false;
    }
    // Declare and initialize variables
    std::string szSendData("Hi,CC-transfer");
    LPVOID ReplyBuffer;
    unsigned int ReplySize = sizeof(ICMP_ECHO_REPLY) + szSendData.size() + 1;
    ReplyBuffer = (LPVOID) malloc(ReplySize);
    if (ReplyBuffer == NULL) {
        return false;
    }
    memset(ReplyBuffer, 0, ReplySize);
    PICMP_ECHO_REPLY pIcmpEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;

    DWORD dwRetVal = funIcmpSendEcho(hIcmpFile, nIP, (void*)(szSendData.c_str()), szSendData.size(), 
        NULL, 
        ReplyBuffer, 
        ReplySize, 
        nTimeout);
    if (dwRetVal != 0) 
    {
        //struct   in_addr addr_in;
        //addr_in = struct   in_addr(pIcmpEchoReply->Address);
        //printf("Received %ld messages.\n", dwRetVal);
        //printf("Data Size: %d\n", pIcmpEchoReply->DataSize);
        //printf("Message: %s\n", pIcmpEchoReply->Data);
        processRecv(pIcmpEchoReply->Status, pIcmpEchoReply->RoundTripTime, pDestIP, sn);
    } else {
        printf("Timeout--->.\n");
        printf("Error: %ld\n", GetLastError());
    }

    free(ReplyBuffer);
    funIcmpCloseHandle(hIcmpFile);
    return true;
#else
    return false;
#endif
	
}

void NePinger::processRecv(unsigned long _status, unsigned long _RTT, const char *pDestIP, unsigned int nSN)
{
	//printf("Received %ld messages.\n", dwRetVal);
	//printf("\n");
	//printf("RTT: %d\n", pIcmpEchoReply->RoundTripTime);
	//printf("Data Size: %d\n", pIcmpEchoReply->DataSize);
	//printf("Message: %s\n", pIcmpEchoReply->Data);
//	unsigned long nIPKey= inet_addr(pDestIP);
	unsigned int nTime = (unsigned int)(-1);
	switch (_status)
	{
	case  0: //IP_SUCCESS:
		{
			nTime = _RTT;
			break;
		}
#ifdef _WIN32
	case IP_DEST_NET_UNREACHABLE:   {	printf("[ERROR] requested : %s; Destination unreachable, Network unreachable\n", pDestIP); break; }
	case IP_DEST_HOST_UNREACHABLE:  {	printf("[ERROR] requested : %s; Destination unreachable, Host unreachable\n", pDestIP); break; }
	case IP_DEST_PROT_UNREACHABLE:  {	printf("[ERROR] requested : %s; Destination unreachable, Protocol unreachable\n", pDestIP); break; }
	case IP_DEST_PORT_UNREACHABLE:  {	printf("[ERROR] requested : %s; Destination unreachable, Port unreachable\n", pDestIP); break; }
	case IP_REQ_TIMED_OUT: { printf("[ERROR] ping: %s ; time-exceed\n", pDestIP); break; }
#endif
	}

	unsigned long nSrcIP = inet_addr("127.0.0.1");
	unsigned long nDestIP= inet_addr(pDestIP);

	m_RouteTable.SetEdgeWeight(nSrcIP, nDestIP, nTime);
}

bool NePinger::Start()
{
	bool bRet = true;
	if (m_hThread != NULL)
	{
		PostTerminate();
		int hr = iposix_thread_join(m_hThread, 0xffffffff);
        if (hr != 0)
        {
            QuickNet::Trace::Global.out(TRACE_ERROR, "[ROUTER]  Fail to Stop Router!.[stime]=%u", iclock());
            return false;
        }
        iposix_thread_delete(m_hThread);
	}
    m_hThread = NULL;

	//m_hThread = CreateThread( NULL,	0, NePinger::WinThreadProc,	this, 0, &m_dwThreadID);
    m_hThread = iposix_thread_new(NePinger::WinThreadProc,	this, "_cc_ip_tunel_router");
    if (m_hThread == NULL)
	{
        QuickNet::Trace::Global.out(TRACE_ERROR, "[ROUTER]  Fail to create router thread!.[stime]=%u", iclock());
		return false;
	}
    m_Lock.write_lock();
    m_bStopWork = false;
    m_Lock.write_unlock();
    int hr = iposix_thread_start(m_hThread);
    if (hr != 0) 
    {
        QuickNet::Trace::Global.out(TRACE_ERROR, "[ROUTER]  Fail to start thread!.[stime]=%u", iclock());
        return false;
    }
	QuickNet::Trace::Global.out(TRACE_ERROR, "[ROUTER] Router start work.[stime]=%u", iclock());

	return bRet;
}

void NePinger::PostTerminate()
{
	m_Lock.write_lock();
	m_bStopWork = true;
    m_Lock.write_unlock();
}

bool NePinger::isWorkStop()
{
    bool bRet = false;
    m_Lock.read_lock();
	bRet = m_bStopWork;
    m_Lock.read_unlock();
    return bRet;
}
