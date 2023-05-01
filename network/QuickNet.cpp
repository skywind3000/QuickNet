//=====================================================================
//       ____        _      __      _   __     __ 
//      / __ \__  __(_)____/ /__   / | / /__  / /_
//     / / / / / / / / ___/ //_/  /  |/ / _ \/ __/
//    / /_/ / /_/ / / /__/ ,<    / /|  /  __/ /_  
//    \___\_\__,_/_/\___/_/|_|  /_/ |_/\___/\__/  
//                                                                 
// QuickNet.cpp - QuickNet 网络接口
//
// NOTE:
// for more information, please see the readme file.
//
//=====================================================================
#ifndef QUICKNET_INTERNAL
#define QUICKNET_EXPORT
#endif

#include "QuickNet.h"
#include "ProtocolImp.h"
#include "../system/inetcode.h"
#include "../system/inetnot.h"


//=====================================================================
// 初始化接口
//=====================================================================
extern "C" int QuickModuleInitialize(void);
extern "C" int QuickModuleActive(const char *text);

static void qnet_server_init(void)
{
#ifdef QUICK_VALIDATE
	if (QuickModuleInitialize() != 0) {
		exit(8);
	}
#endif
}


//=====================================================================
// 验证接口
//=====================================================================
extern "C" QNETAPI int quicknet_active(const char *activecode)
{
#ifdef QUICK_VALIDATE
	return QuickModuleActive(activecode);
#else
	return 0;
#endif
}


//=====================================================================
// 服务端接口
//=====================================================================

// 新建服务端
QNETAPI QNetServer qnet_server_new(void)
{
	QuickNet::QuickServer *server = new QuickNet::QuickServer;
	qnet_server_init();
	return (QNetServer)server;
}

// 删除服务端
QNETAPI void qnet_server_delete(QNetServer server)
{
	QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
	delete svr;
}

// 开始服务，成功返回0，失败返回-1
QNETAPI int qnet_server_startup(QNetServer server, int port, const char *ip)
{
	QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
	return svr->StartService(port, ip)? 0 : -1;
}

// 关闭服务
QNETAPI void qnet_server_shutdown(QNetServer server)
{
	QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
	svr->Shutdown();
}

// 更新服务
QNETAPI void qnet_server_update(QNetServer server)
{
	QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
	svr->Update();
}

// 关闭客户端
QNETAPI void qnet_server_close(QNetServer server, unsigned long hid, int code)
{
	QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
	svr->Close(hid, code);
}

// 发送数据
QNETAPI int qnet_server_send(QNetServer server, unsigned long hid, 
	int protocol, const char *data, int size)
{
	QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
	return svr->Send(hid, protocol, data, size, -1)? 1 : 0;
}

// 发送数据，limit <= 0为不限制，否则将限制发送缓存
QNETAPI int qnet_server_send_limit(QNetServer server, unsigned long hid, 
	int protocol, const char *data, int size, int limit)
{
	QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
	return svr->Send(hid, protocol, data, size, limit);
}


// 取得消息，成功返回消息长度，返回-1为没有消息，-2为长度错误
// 如果 event, wparam, lparam, 任意为 NULL则返回消息长度
QNETAPI int qnet_server_read(QNetServer server, int *event, 
	unsigned long *wparam, unsigned long *lparam, void *data, int size)
{
	QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
	IUINT32 wp, lp;
	int hr = svr->Read(event, &wp, &lp, data, size);
	*wparam = (unsigned long)wp;
	*lparam = (unsigned long)lp;
	return hr;
}


// 设置超时
// idle是空闲超时，-1为不设置，0为取消空闲超时，>0为毫秒时间，超过这个时间收不到包的客户端断开
// connect连接超时，-1为不设置，0为取消连接超时，>0为毫秒时间，超过这个时间没连接成功的客户端断开
QNETAPI void qnet_server_timeout(QNetServer server, int idle, int connect)
{
	QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
	svr->SetTimeOut(idle, connect);
}

// 设置更新 interval
QNETAPI void qnet_server_interval(QNetServer server, int interval)
{
	QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
	svr->SetInterval(interval);
}

// 配置网络
QNETAPI int qnet_server_option(QNetServer server, unsigned long hid, const char *options)
{
	QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
	return svr->Option(hid, options);
}

QNETAPI int qnet_server_get_option(QNetServer server, unsigned long hid, int option)
{
    QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
    if (svr == NULL)
    {
        return -1;
    }
    return svr->GetOption(hid, option);
}

// 取得连接状态：0关闭，1连接1，2连接2，3连接等待，4建立连接，5关闭等待。-1为错误 hid
QNETAPI int qnet_server_get_state(const QNetServer server, unsigned long hid)
{
	const QuickNet::QuickServer *svr = (const QuickNet::QuickServer*)server;
	return svr->GetState(hid);
}

// 广播一群人，返回成功个数
QNETAPI int qnet_server_groupcast1(QNetServer server, unsigned long *hid, int count,
	int protocol, const void *data, int size, int limit)
{
	QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
	int ok = 0;
	for (int i = 0; i < count; i++) {
		if (svr->Send((IUINT32)hid[i], protocol, data, size, limit)) ok++;
	}
	return ok;
}

// 广播一群人，返回成功个数
QNETAPI int qnet_server_groupcast2(QNetServer server, unsigned int *hid, int count,
	int protocol, const void *data, int size, int limit)
{
	QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
	int ok = 0;
	for (int i = 0; i < count; i++) {
		if (svr->Send((IUINT32)hid[i], protocol, data, size, limit)) ok++;
	}
	return ok;
}

// 广播所有人，返回成功个数
QNETAPI int qnet_server_broadcast(QNetServer server, int protocol, const void *data,
	int size, int limit)
{
	QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
	return svr->Broadcast(protocol, data, size, limit);
}

// 设置全局掩码：
QNETAPI void qnet_server_setgmask(QNetServer server, unsigned int mask)
{
	QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
	svr->SetGlobalMask(mask);
}

// 取得待发送数据
QNETAPI int qnet_server_get_pending(QNetServer server, unsigned long hid, int what)
{
	QuickNet::QuickServer *svr = (QuickNet::QuickServer*)server;
	return svr->GetPending((IUINT32)hid, what);
}


//=====================================================================
// 客户端接口
//=====================================================================

// 新建客户端
QNETAPI QNetClient qnet_client_new(void)
{
	QuickNet::QuickClient *client = new QuickNet::QuickClient;
	return (QNetClient*)client;
}

// 删除客户端
QNETAPI void qnet_client_delete(QNetClient client)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	delete c;
}

// 连接远程服务端
QNETAPI int qnet_client_connect(QNetClient client, const char *ip, int port)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
    if (c != NULL)
    {
        c->GetPingRouter().SetDestHostIP(ip);
        c->GetPingRouter().SetDestPort(port);
        return c->Connect(ip, port);
    }
    return -1;
	
}

//设置Cuteid,为pingrouter 统计信息用
QNETAPI void qnet_client_setcuteid(QNetClient client, unsigned int id)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	if (c != NULL)
	{
		c->GetPingRouter().SetCuteID(id);
	}
}


QNETAPI  void qnet_client_setcgihost(QNetClient client, const char* ip, int port)
{
    QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
    if (c != NULL)
    {
        c->GetPingRouter().SetCGIHost(ip, port);
    }
}

QNETAPI void qnet_client_setpingtimes(QNetClient client, int ntimes)
{
    QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
    if (c != NULL)
    {
        c->GetPingRouter().SetPingTimes(ntimes);
    }
}
QNETAPI void qnet_client_setpingtimeout(QNetClient client, int nMiliSeconds)
{
    QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
    if (c != NULL)
    {
        c->GetPingRouter().SetPingTimeout(nMiliSeconds);
    }
}
QNETAPI void qnet_client_setpingsleep(QNetClient client, int nMiliSeconds)
{
    QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
    if (c != NULL)
    {
        c->GetPingRouter().SetPingSleep(nMiliSeconds);
    }
}
QNETAPI unsigned int qnet_client_getcuteid(QNetClient client)
{
    unsigned int nRet = 0;
    QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
    if (c != NULL)
    {
        nRet = c->GetPingRouter().GetCuteID();
    }
    return nRet;
}

//开始一次router only once;
QNETAPI void qnet_client_startrouter(QNetClient client)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	if (c != NULL)
	{
        if (c->GetPingRouter().IsReady())
        {
            c->GetPingRouter().Start();
        }
	}
}

// 关闭连接
QNETAPI void qnet_client_close(QNetClient client)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	c->Close();
}

// 发送数据
QNETAPI int qnet_client_send(QNetClient client, int protocol, 
	const void *data, int size)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	return c->Send(protocol, data, size);
}

// 发送数据并限制大小
QNETAPI int qnet_client_send_limit(QNetClient client, int protocol, 
	const void *data, int size, int limit)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	return c->Send(protocol, data, size, limit);
}

// 接收数据
QNETAPI int qnet_client_recv(QNetClient client, int *protocol, 
	void *data, int size)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	return c->Recv(protocol, data, size);
}

// 更新状态
QNETAPI void qnet_client_update(QNetClient client)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	c->Update();
}

// 设置保活
QNETAPI void qnet_client_keepalive(QNetClient client, int interval)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	c->Keepalive(interval);
}

// 配置连接
QNETAPI int qnet_client_option(QNetClient client, const char *options)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	return c->Option(options);
}

QNETAPI int qnet_client_get_option(QNetClient client, int option)
{
    QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
    return c->GetOption(option);
}

// 是否连接：0为没有连接，1为连接了
QNETAPI int qnet_client_is_connected(const QNetClient client)
{
	const QuickNet::QuickClient *c = (const QuickNet::QuickClient*)client;
	return c->IsConnected();
}

// 取得状态：0关闭，1连接1，2连接2，3连接等待，4建立连接，5关闭等待。
QNETAPI int qnet_client_get_state(const QNetClient client)
{
	const QuickNet::QuickClient *c = (const QuickNet::QuickClient*)client;
	return c->GetState();
}

// 取得 rtt
QNETAPI int qnet_client_get_rtt(const QNetClient client)
{
	const QuickNet::QuickClient *c = (const QuickNet::QuickClient*)client;
	return c->GetRtt();
}


// 获取拉包层统计信息
QNETAPI void qnet_client_get_nack_stat_info(QNetClient client, int* packets, int *pull,
	int* pullpkts, int *lost, int *pulltimeout, int *skip, int *totalskippkt)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	return c->GetNACKStatInfo(packets, pull, pullpkts, lost, pulltimeout, skip, totalskippkt);
}

// 切换目标 IP:PORT 地址
QNETAPI int qnet_client_switch_target(QNetClient client, const char *ip, int port)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	return c->SwitchTargetAddress(ip, port)? 0 : -1;
}

// 取得目标 IP:PORT 地址
QNETAPI int qnet_client_get_target(QNetClient client, char *ip, int *port)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	System::SockAddress target;
	target.set(ip, *port);
	bool hr = c->GetTargetAddress(target);
	target.get_ip_text(ip);
	*port = target.get_port();
	return hr? 0 : -1;
}

// 设置全局掩码：
QNETAPI void qnet_client_setgmask(QNetClient client, unsigned int mask)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	c->SetGlobalMask(mask);
}

// 设置超时
QNETAPI void qnet_client_set_timeout(const QNetClient client, int idle, int connect)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	c->SetTimeOut(idle, connect);
}

// 取得待发送数据
QNETAPI int qnet_client_get_pending(QNetClient client, int what)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	return c->GetPending(what);
}

// 收发流量统计
QNETAPI void qnet_client_statistic(QNetClient client, struct QNetStatistic *stat)
{
	QuickNet::QuickClient *c = (QuickNet::QuickClient*)client;
	QuickNet::ProtocolUdp::Statistic s;
	c->Statistic(s);
	stat->out_count = s.out_count;
	stat->out_data = s.out_data;
	stat->out_size = s.out_size;
	stat->in_count = s.in_count;
	stat->in_data = s.in_data;
	stat->in_size = s.in_size;
	stat->discard_count = s.discard_count;
	stat->discard_data = s.discard_data;
	stat->discard_size = s.discard_size;
	stat->per_sec_out_count = (int)s.per_sec_out_count;
	stat->per_sec_out_data = (int)s.per_sec_out_data;
	stat->per_sec_out_size = (int)s.per_sec_out_size;
	stat->per_sec_in_count = (int)s.per_sec_in_count;
	stat->per_sec_in_data = (int)s.per_sec_in_data;
	stat->per_sec_in_size = (int)s.per_sec_in_size;
	stat->per_sec_discard_count = (int)s.per_sec_discard_count;
	stat->per_sec_discard_data = (int)s.per_sec_discard_data;
	stat->per_sec_discard_size = (int)s.per_sec_discard_size;
}


//=====================================================================
// 日志接口
//=====================================================================

// 设置日志输出函数
QNETAPI void qnet_trace_setout(qnet_trace_proc proc, void *user)
{
	QuickNet::Trace::Global.setout(proc, user);
}

// 打开文件
QNETAPI void qnet_trace_open(const char *prefix, int usestdout)
{
	QuickNet::Trace::Global.open(prefix, usestdout? true : false);
}

// 关闭文件
QNETAPI void qnet_trace_close(void)
{
	QuickNet::Trace::Global.close();
}

// 设置掩码
QNETAPI void qnet_trace_setmask(int mask)
{
	QuickNet::Trace::Global.setmask(mask);
}

// 允许掩码
QNETAPI void qnet_trace_enable(int mask)
{
	QuickNet::Trace::Global.enable(mask);
}

// 禁止掩码
QNETAPI void qnet_trace_disable(int mask)
{
	QuickNet::Trace::Global.disable(mask);
}

// 设置日志颜色
QNETAPI int qnet_trace_color(int color)
{
	return QuickNet::Trace::Global.color(color);
}

// 输出日志
QNETAPI void qnet_trace_out(int mask, const char *text)
{
	QuickNet::Trace::Global.out(mask, "%s", text);
}

// 检测是否合法
QNETAPI int qnet_trace_available(int mask)
{
	return QuickNet::Trace::Global.available(mask);
}



//=====================================================================
// 帮助 Python计时的东西，比 sleep更精确的控制
//=====================================================================
extern "C" {
QNETAPI iPosixTimer* qnet_timer_new(void) 
{
	return iposix_timer_new();
}

QNETAPI void qnet_timer_delete(iPosixTimer *timer)
{
	assert(timer);
	iposix_timer_delete(timer);
}

/* start timer, delay is millisec, returns zero for success */
QNETAPI int qnet_timer_start(iPosixTimer *timer, unsigned long delay, int periodic) 
{
	return iposix_timer_start(timer, delay, periodic);
}

/* stop timer */
QNETAPI void qnet_timer_stop(iPosixTimer *timer)
{
	return iposix_timer_stop(timer);
}

/* wait, returns 1 for timer, otherwise for timeout */
QNETAPI int qnet_timer_wait_time(iPosixTimer *timer, unsigned long millisec)
{
	return iposix_timer_wait_time(timer, millisec);
}

/* wait infinite */
QNETAPI int qnet_timer_wait(iPosixTimer *timer)
{
	return iposix_timer_wait(timer);
}

/* timer signal set */
QNETAPI int qnet_timer_set(iPosixTimer *timer)
{
	return iposix_timer_set(timer);
}

/* timer signal reset */
QNETAPI int qnet_timer_reset(iPosixTimer *timer)
{
	return iposix_timer_reset(timer);
}

}


//=====================================================================
// TCP非阻塞接口
//=====================================================================
QNETAPI AsyncSock* qnet_tcp_new(void) {
	CAsyncSock *sock = (CAsyncSock*)malloc(sizeof(CAsyncSock));
	async_sock_init(sock, NULL);
	return sock;
}

QNETAPI void qnet_tcp_delete(AsyncSock *sock) {
	async_sock_destroy((CAsyncSock*)sock);
	free(sock);
}

QNETAPI int qnet_tcp_connect(AsyncSock *sock, const char *ip, int port, int head) {
	sockaddr rmt;
	isockaddr_makeup(&rmt, ip, port);
	return async_sock_connect((CAsyncSock*)sock, &rmt, 0, head);
}

QNETAPI int qnet_tcp_assign(AsyncSock *sock, int fd, int head) {
	return async_sock_assign((CAsyncSock*)sock, fd, head);
}

QNETAPI void qnet_tcp_close(AsyncSock *sock) {
	async_sock_close((CAsyncSock*)sock);
}


QNETAPI int qnet_tcp_state(const AsyncSock *sock) {
	return async_sock_state((const CAsyncSock*)sock);
}

QNETAPI int qnet_tcp_fd(const AsyncSock *sock) {
	return async_sock_fd((const CAsyncSock*)sock);
}

QNETAPI long qnet_tcp_remain(const AsyncSock *sock) {
	return async_sock_remain((const CAsyncSock*)sock);
}

// send data
QNETAPI long qnet_tcp_send(AsyncSock *sock, const void *ptr, long size, int mask) {
	return async_sock_send((CAsyncSock*)sock, ptr, size, mask);
}

// recv vector: returns packet size, -1 for not enough data, -2 for 
// buffer size too small, -3 for packet size error, -4 for size over limit,
// returns packet size if ptr equals NULL.
QNETAPI long qnet_tcp_recv(AsyncSock *sock, void *ptr, int size) {
	return async_sock_recv((CAsyncSock*)sock, ptr, size);
}

// send vector
QNETAPI long qnet_tcp_send_vector(AsyncSock *sock, const void *vecptr[],
	const long veclen[], int count, int mask) {
	return async_sock_send_vector((CAsyncSock*)sock, vecptr, veclen, count, mask);
}

// recv vector: returns packet size, -1 for not enough data, -2 for 
// buffer size too small, -3 for packet size error, -4 for size over limit,
// returns packet size if vecptr equals NULL.
QNETAPI long qnet_tcp_recv_vector(AsyncSock *sock, void *vecptr[], 
	const long veclen[], int count) {
	return async_sock_recv_vector((CAsyncSock*)sock, vecptr, veclen, count);
}

// update
QNETAPI int qnet_tcp_update(AsyncSock *sock, int what) {
	return async_sock_update((CAsyncSock*)sock, what);
}

// process
QNETAPI void qnet_tcp_process(AsyncSock *sock) {
	async_sock_process((CAsyncSock*)sock);
}


// set send cryption key
QNETAPI void qnet_tcp_rc4_set_skey(AsyncSock *sock, const unsigned char *key, 
	int keylen) {
	async_sock_rc4_set_skey((CAsyncSock*)sock, key, keylen);
}

// set recv cryption key
QNETAPI void qnet_tcp_rc4_set_rkey(AsyncSock *sock, const unsigned char *key, 
	int keylen) {
	async_sock_rc4_set_rkey((CAsyncSock*)sock, key, keylen);
}

// set nodelay
QNETAPI int qnet_tcp_nodelay(AsyncSock *sock, int nodelay) {
	return async_sock_nodelay((CAsyncSock*)sock, nodelay);
}

// set buf size
QNETAPI int qnet_tcp_sys_buffer(AsyncSock *sock, long rcvbuf, long sndbuf) {
	return async_sock_sys_buffer((CAsyncSock*)sock, rcvbuf, sndbuf);
}

// set keepalive
QNETAPI int qnet_tcp_keepalive(AsyncSock *sock, int keepcnt, int idle, int intvl) {
	return async_sock_keepalive((CAsyncSock*)sock, keepcnt, idle, intvl);
}



//---------------------------------------------------------------------
// TCP 异步事件管理器
//---------------------------------------------------------------------

// new AsyncCore object
QNETAPI AsyncCore* qnet_async_new(void) {
	qnet_server_init();
	return (AsyncCore*)async_core_new(0);
}

// delete async core
QNETAPI void qnet_async_delete(AsyncCore *core) {
	async_core_delete((CAsyncCore*)core);
}


// wait for events for millisec ms. and process events, 
// if millisec equals zero, no wait.
QNETAPI void qnet_async_wait(AsyncCore *core, unsigned long millisec) {
	async_core_wait((CAsyncCore*)core, millisec);
}

// wake-up from qnet_async_wait
QNETAPI void qnet_async_notify(AsyncCore *core) {
	async_core_notify((CAsyncCore*)core);
}

// read events, returns data length of the message, 
// and returns -1 for no event, -2 for buffer size too small,
QNETAPI long qnet_async_read(AsyncCore *core, int *event, long *wparam,
	long *lparam, void *data, long size) {
	return async_core_read((CAsyncCore*)core, event, wparam, lparam, data, size);
}


// send data to given hid
QNETAPI long qnet_async_send(AsyncCore *core, long hid, const void *ptr, long len) {
	return async_core_send((CAsyncCore*)core, hid, ptr, len);
}

// close given hid
QNETAPI int qnet_async_close(AsyncCore *core, long hid, int code) {
	return async_core_close((CAsyncCore*)core, hid, code);
}

// send vector
QNETAPI long qnet_async_send_vector(AsyncCore *core, long hid, const void *vecptr[],
	const long veclen[], int count, int mask) {
	return async_core_send_vector((CAsyncCore*)core, hid, vecptr, veclen, count, mask);
}

// send data with mask
QNETAPI long qnet_async_send_mask(AsyncCore *core, long hid, const void *ptr, long len,
	int mask)
{
	const void *vecptr[1];
	long veclen[1];
	vecptr[0] = ptr;
	veclen[0] = len;
	return async_core_send_vector((CAsyncCore*)core, hid, vecptr, veclen, 1, mask);
}


// new connection to the target address, returns hid
QNETAPI long qnet_async_new_connect(AsyncCore *core, const char *ip, int port, int header) {
	sockaddr rmt;
	isockaddr_makeup(&rmt, ip, port);
	return async_core_new_connect((CAsyncCore*)core, &rmt, 0, header);
}

// new listener, returns hid
QNETAPI long qnet_async_new_listen(AsyncCore *core, const char *ip, int port, int header) {
	sockaddr rmt;
	isockaddr_makeup(&rmt, ip, port);
	return async_core_new_listen((CAsyncCore*)core, &rmt, 0, header);
}

// new assign, returns hid
QNETAPI long qnet_async_new_assign(AsyncCore *core, int fd, int header, int check_estab) {
	return async_core_new_assign((CAsyncCore*)core, fd, header, check_estab);
}

// get node mode: ASYNCCORE_NODE_IN/OUT/LISTEN4/LISTEN6
QNETAPI int qnet_async_get_mode(const AsyncCore *core, long hid) {
	return async_core_get_mode((const CAsyncCore*)core, hid);
}

// returns connection tag, -1 for hid not exist
QNETAPI long qnet_async_get_tag(const AsyncCore *core, long hid) {
	return async_core_get_tag((const CAsyncCore*)core, hid);
}

// set connection tag
QNETAPI void qnet_async_set_tag(AsyncCore *core, long hid, long tag) {
	return async_core_set_tag((CAsyncCore*)core, hid, tag);
}

// get send queue size
QNETAPI long qnet_async_remain(const AsyncCore *core, long hid) {
	return async_core_remain((const CAsyncCore*)core, hid);
}

// set default buffer limit and max packet size
QNETAPI void qnet_async_limit(AsyncCore *core, long limited, long maxsize) {
	async_core_limit((CAsyncCore*)core, limited, maxsize);
}



// get first node
QNETAPI long qnet_async_node_head(const AsyncCore *core) {
	return async_core_node_head((const CAsyncCore*)core);
}

// get next node
QNETAPI long qnet_async_node_next(const AsyncCore *core, long hid) {
	return async_core_node_next((const CAsyncCore*)core, hid);
}

// get prev node
QNETAPI long qnet_async_node_prev(const AsyncCore *core, long hid) {
	return async_core_node_prev((const CAsyncCore*)core, hid);
}

// set connection socket option
QNETAPI int qnet_async_option(AsyncCore *core, long hid, int opt, long value) {
	return async_core_option((CAsyncCore*)core, hid, opt, value);
}

// set connection rc4 send key
QNETAPI int qnet_async_rc4_set_skey(AsyncCore *core, long hid, 
	const unsigned char *key, int keylen) {
	return async_core_rc4_set_skey((CAsyncCore*)core, hid, key, keylen);
}

// set connection rc4 recv key
QNETAPI int qnet_async_rc4_set_rkey(AsyncCore *core, long hid,
	const unsigned char *key, int keylen) {
	return async_core_rc4_set_rkey((CAsyncCore*)core, hid, key, keylen);
}

// set remote ip validator
QNETAPI void qnet_async_firewall(AsyncCore *core, AsyncValidator v, void *user) {
	return async_core_firewall((CAsyncCore*)core, (CAsyncValidator)v, user);
}

// set timeout
QNETAPI void qnet_async_timeout(AsyncCore *core, long seconds) {
	return async_core_timeout((CAsyncCore*)core, seconds);
}

// get sockname
QNETAPI int qnet_async_sockname(const AsyncCore *core, long hid, char *out) {
	System::SockAddress remote;
	int hr = async_core_sockname((CAsyncCore*)core, hid, remote.address(), NULL);
	if (hr != 0) {
		out[0] = 0;
		return hr;
	}
	remote.string(out);
	return hr;
}

// get peername
QNETAPI int qnet_async_peername(const AsyncCore *core, long hid, char *out) {
	System::SockAddress remote;
	int hr = async_core_peername((CAsyncCore*)core, hid, remote.address(), NULL);
	if (hr != 0) {
		out[0] = 0;
		return hr;
	}
	remote.string(out);
	return hr;
}

// disable read poll event
QNETAPI int qnet_async_disable(AsyncCore *core, long hid, int value)
{
	return async_core_disable((CAsyncCore*)core, hid, value);
}

// get connection socket status
QNETAPI int qnet_async_status(AsyncCore *core, long hid, int opt)
{
	return (int)async_core_status((CAsyncCore*)core, hid, opt);
}

// get number of connections
QNETAPI int qnet_async_nfds(const AsyncCore *core)
{
	return (int)async_core_nfds((const CAsyncCore*)core);
}

// queue an ASYNC_CORE_EVT_PUSH event and wake async_core_wait up
QNETAPI int qnet_async_post(AsyncCore *core, long wparam, long lparam, const char *data, long size) {
	return async_core_post((CAsyncCore*)core, wparam, lparam, data, size);
}


namespace QuickNet {
//=====================================================================
// 简易客户端接口
//=====================================================================
struct QuickImp : public QuickSocket
{
public:
	virtual ~QuickImp() {}
	void Release() { delete this; }
	int connect(const char *ip, int port) { return c.connect(ip, port); }
	void close() { c.close(); }
	int send(int protocol, const void *data, int size, int limit) { return c.send(protocol, data, size, limit); }
	int recv(int *protocol, void *data, int size) { return c.recv(protocol, data, size); }
	void update() { c.update(); }
	void keepalive(int interval) { c.keepalive(interval); }
	int option(const char *options) { return c.option(options); }
	int getoption(int option) { return c.getoption(option); }
	int isconnected() const { return c.isconnected(); }
	int state() const { return c.state(); }
	int getrtt() const { return c.getrtt(); }
	int target_switch(const char *ip, int port) { return c.target_switch(ip, port); }
	int target_get(char *ip, int *port) { return c.target_get(ip, port); }
	void set_gmask(unsigned int mask) { return c.set_gmask(mask); }

	void getnackstatinfo(int* packets, int *pull, int* pullpkts, 
		int *lost, int *pulltimeout, int *skip, int *totalskippkt) {
		return c.getnackstatinfo(packets, pull, pullpkts, 
			lost, pulltimeout, skip, totalskippkt);
	}

protected:
	QuickNet::Client c;
};


}


//=====================================================================
// 调试接口
//=====================================================================
QNETAPI int qnet_debug_page_count(void)
{
	ilong inuse = 0, pn = 0, pd = 0;
	/* ikmem_page_info(&inuse, &pn, &pd); */
	return inuse;
}

QNETAPI int qnet_debug_disable_kmem(void)
{
	/* ikmem_hook_install(ikmem_hook_get(1)); */
	return 0;
}

QNETAPI QuickNet::QuickSocket* QuickSocket_Create(void)
{
	return new QuickNet::QuickImp();
}



//=====================================================================
// AsyncNotify
//=====================================================================
// create object
QNETAPI AsyncNotify* qnet_notify_new(int serverid) {
	qnet_server_init();
	return (AsyncNotify*)async_notify_new(serverid);
}

// delete object
QNETAPI void qnet_notify_delete(AsyncNotify *notify) {
	CAsyncNotify *self = (CAsyncNotify*)notify;
	QuickNet::Trace *trace = (QuickNet::Trace*)async_notify_user(self, NULL);
	if (trace) {
		delete trace;
	}
	async_notify_delete(self);
}

// wait events
QNETAPI void qnet_notify_wait(AsyncNotify *notify, unsigned long millisec) {
	async_notify_wait((CAsyncNotify*)notify, millisec);
}

// wake-up from waiting
QNETAPI void qnet_notify_wake(AsyncNotify *notify) {
	async_notify_wake((CAsyncNotify*)notify);
}

// read events, returns data length of the message, 
// and returns -1 for no event, -2 for buffer size too small,
// returns data size when data equals NULL.
QNETAPI long qnet_notify_read(AsyncNotify *notify, int *event, long *wparam,
	long *lparam, void *data, long maxsize) {
	return async_notify_read((CAsyncNotify*)notify, event, wparam, lparam, data, maxsize);
}


// new listen: return id(-1 error, -2 port conflict), flag&1(reuse)
QNETAPI long qnet_notify_listen(AsyncNotify *notify, const char *addr, int port, int flag) {
	System::SockAddress remote(addr, port);
	return async_notify_listen((CAsyncNotify*)notify, remote.address(), 
		sizeof(sockaddr), flag);
}

// remove listening port
QNETAPI int qnet_notify_remove(AsyncNotify *notify, long listenid, int code) {
	return async_notify_remove((CAsyncNotify*)notify, listenid, code);
}

// setup self server id
QNETAPI void qnet_notify_change(AsyncNotify *notify, int new_server_id) {
	async_notify_change((CAsyncNotify*)notify, new_server_id);
}

// send message to server
QNETAPI int qnet_notify_send(AsyncNotify *notify, int sid, short cmd, 
	const void *data, long size) {
	return async_notify_send((CAsyncNotify*)notify, sid, cmd, data, size);
}

// close server connection
QNETAPI int qnet_notify_close(AsyncNotify *notify, int sid, int mode, int code) {
	return async_notify_close((CAsyncNotify*)notify, sid, mode, code);
}

// get listening port
QNETAPI int qnet_notify_get_port(AsyncNotify *notify, long listenid) {
	return async_notify_get_port((CAsyncNotify*)notify, listenid);
}

// clear ip allow table
QNETAPI void qnet_notify_allow_clear(AsyncNotify *notify) {
	async_notify_allow_clear((CAsyncNotify*)notify);
}

// add or update ip in allow table
QNETAPI void qnet_notify_allow_add(AsyncNotify *notify, const char *ip) {
	System::SockAddress remote(ip, 0);
	struct sockaddr_in *addr = (struct sockaddr_in*)remote.address();
	async_notify_allow_add((CAsyncNotify*)notify, &(addr->sin_addr.s_addr), 4);
}

// remove ip from table
QNETAPI void qnet_notify_allow_del(AsyncNotify *notify, const char *ip) {
	System::SockAddress remote(ip, 0);
	struct sockaddr_in *addr = (struct sockaddr_in*)remote.address();
	async_notify_allow_del((CAsyncNotify*)notify, &(addr->sin_addr.s_addr), 4);
}

// ip table enable: enable is 0(disable allow table) otherwise(enable)
QNETAPI void qnet_notify_allow_enable(AsyncNotify *notify, int enable) {
	async_notify_allow_enable((CAsyncNotify*)notify, enable);
}


// add or update a sid into sid2addr
QNETAPI void qnet_notify_sid_add(AsyncNotify *notify, int sid, const char *ip, int port) {
	System::SockAddress remote(ip, port);
	async_notify_sid_add((CAsyncNotify*)notify, sid, remote.address(), sizeof(sockaddr));
}

// add or update a sid into sid2addr
QNETAPI void qnet_notify_sid_del(AsyncNotify *notify, int sid) {
	async_notify_sid_del((CAsyncNotify*)notify, sid);
}

// list sids into an array
QNETAPI int qnet_notify_sid_list(AsyncNotify *notify, int *sids, int maxsize) {
	return async_notify_sid_list((CAsyncNotify*)notify, sids, maxsize);
}

// clear sid list
QNETAPI void qnet_notify_sid_clear(AsyncNotify *notify) {
	async_notify_sid_clear((CAsyncNotify*)notify);
}

// config
QNETAPI int qnet_notify_option(AsyncNotify *notify, int type, long value) {
	return async_notify_option((CAsyncNotify*)notify, type, value);
}

// set login token
QNETAPI void qnet_notify_token(AsyncNotify *notify, const char *token, int size) {
	async_notify_token((CAsyncNotify*)notify, token, size);
}

void qnet_notify_writelog(const char *text, void *user) {
	QuickNet::Trace *trace = (QuickNet::Trace*)user;
	if (user) {
		trace->out(1, text);
	}
}

// set logging
QNETAPI void qnet_notify_trace(AsyncNotify *notify, const char *prefix, int STDOUT, int color)
{
	CAsyncNotify *self = (CAsyncNotify*)notify;
	QuickNet::Trace *trace = new QuickNet::Trace(prefix, STDOUT? true : false, color);
	trace->open(prefix, STDOUT? true : false);
	trace->setmask(1);
	trace = (QuickNet::Trace*)async_notify_user(self, trace);
	if (trace) delete trace;
	async_notify_install(self, qnet_notify_writelog);
}



// system 目录代码加入工程：
//! src: ../system/imembase.c, ../system/imemdata.c, ../system/inetbase.c, ../system/inetcode.c
//! src: ../system/ctiming.c, ../system/inetsim.c, ../system/ilog.c, ../system/cprofile.c, 
//! src: ../system/inetkcp.c, ../system/inettcp.c, ../system/iposix.c, ../system/itoolbox.c
//! src: ../system/fec.c, ../system/ineturl.c, ../system/inetnot.c

//! src: TransportUdp.cpp, ProtocolBasic.cpp, ProtocolImp.cpp, SessionDesc.cpp, SessionManager.cpp,
//! src: FecCodec.cpp, FecCodecBuf.cpp, FecPacket.cpp, FecTransmission.cpp, NetFecCodec.cpp
//! src: RequestRepeat.cpp, NePingRouter.cpp, QuickValidate.cpp, Combinator.cpp

// 编译选项
//! flag: -g, -O3, -Wall
//! mode: dll
//! int: ../build/objs
//! out: ../depends/QuickNet
//! link: stdc++

//! win32/out: ../depends/QuickNet.win32
//! freebsd6/out: ../depends/QuickNet.freebsd6
//! freebsd7/out: ../depends/QuickNet.freebsd7
//! linux2/out: ../depends/QuickNet.linux2
//! linux3/out: ../depends/QuickNet.linux3
//! darwin/out: ../depends/QuickNet.darwin
//! cygwin/out: ../depends/QuickNet.cygwin

// export: def, lib, msvc

//! color: 13
//! echo:      ____        _      __      _   __     __ 
//! echo:     / __ \__  __(_)____/ /__   / | / /__  / /_
//! echo:    / / / / / / / / ___/ //_/  /  |/ / _ \/ __/
//! echo:   / /_/ / /_/ / / /__/ ,<    / /|  /  __/ /_  
//! echo:   \___\_\__,_/_/\___/_/|_|  /_/ |_/\___/\__/  
//! echo:
//! color: -1


