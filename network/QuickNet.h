//=====================================================================
//       ____        _      __      _   __     __ 
//      / __ \__  __(_)____/ /__   / | / /__  / /_
//     / / / / / / / / ___/ //_/  /  |/ / _ \/ __/
//    / /_/ / /_/ / / /__/ ,<    / /|  /  __/ /_  
//    \___\_\__,_/_/\___/_/|_|  /_/ |_/\___/\__/  
//                                                                 
// QuickNet.h - QuickNet 网络接口
//
// NOTE:
// for more information, please see the readme file.
//
//=====================================================================
#ifndef __QUICKNET_H__
#define __QUICKNET_H__


// 如果定义了 QUICKNET_INTERNAL的话，就没有导出导入接口
#ifdef QUICKNET_INTERNAL
	#define QNETAPI 
#elif defined(_WIN32) || defined(WIN32) || defined(WIN64) || defined(_WIN64)
	#ifndef QUICKNET_EXPORT
		#define QNETAPI __declspec( dllimport )
	#else
		#define QNETAPI __declspec( dllexport )
	#endif
#else
	#define QNETAPI
#endif



#ifdef __cplusplus
extern "C" {
#endif

#define TRACE_MASK_ERROR			1
#define TRACE_MASK_WARNING			2
#define TRACE_MASK_MGR_PACKET		4
#define TRACE_MASK_MGR_SYN			8
#define TRACE_MASK_MGR_EVENT		16
#define TRACE_MASK_SESSION			32
#define TRACE_MASK_KCP				64
#define TRACE_MASK_SERVER			128
#define TRACE_MASK_CLIENT			256

//=====================================================================
// 全局定义
//=====================================================================
typedef void* QNetServer;
typedef void* QNetClient;

#define QNET_PROTOCOL_RAW		0		// 协议：原始 UDP
#define QNET_PROTOCOL_KCP		1		// 协议：KCP协议
#define QNET_PROTOCOL_TCP		2		// 协议：TCP协议
#define QNET_PROTOCOL_NACK		3		// 协议：NACK协议
#define QNET_PROTOCOL_FEC_RAW	4		// 协议：FEC + 原始 UDP
#define QNET_PROTOCOL_FEC_KCP	5		// 协议：FEC + KCP
#define QNET_PROTOCOL_FEC_TCP	6		// 协议：FEC + TCP
#define QNET_PROTOCOL_FEC_NACK	7		// 协议：FEC + NACK


//=====================================================================
// 流量统计
//=====================================================================
#ifndef __IINT64_DEFINED
#define __IINT64_DEFINED
#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef __int64 IINT64;
#else
typedef long long IINT64;
#endif
#endif

struct QNetStatistic
{
	IINT64 out_count;
	IINT64 out_size;
	IINT64 out_data;
	IINT64 in_count;
	IINT64 in_size;
	IINT64 in_data;
	IINT64 discard_count;
	IINT64 discard_size;
	IINT64 discard_data;
	int per_sec_out_count;
	int per_sec_out_size;
	int per_sec_out_data;
	int per_sec_in_count;
	int per_sec_in_size;
	int per_sec_in_data;
	int per_sec_discard_count;
	int per_sec_discard_size;
	int per_sec_discard_data;
};


//=====================================================================
// 服务端接口
//=====================================================================

// 新建服务端
QNETAPI QNetServer qnet_server_new(void);

// 新建客户端
QNETAPI void qnet_server_delete(QNetServer server);

// 开始服务，成功返回0，失败返回-1
QNETAPI int qnet_server_startup(QNetServer server, int port, const char *ip);

// 关闭服务
QNETAPI void qnet_server_shutdown(QNetServer server);



// 更新服务，需要循环调用，最好 10ms调用一次
QNETAPI void qnet_server_update(QNetServer server);


// 关闭某连接，hid为连接编号，code为断开代码（记录到日志，方便查询）
QNETAPI void qnet_server_close(QNetServer server, unsigned long hid, int code);


// 发送数据：成功返回1，失败返回0，hid是连接编号，protocol是协议(0-3)，data是数据
QNETAPI int qnet_server_send(QNetServer server, unsigned long hid, 
	int protocol, const char *data, int size);


// 发送数据，limit <= 0为不限制，否则将限制发送缓存，成功返回1，失败返回0
// 如果 limit > 0，并且该连接的发送缓存还有 >=limit个待发数据包，则放弃本次发送
QNETAPI int qnet_server_send_limit(QNetServer server, unsigned long hid, 
	int protocol, const char *data, int size, int limit);


#define QNET_EVENT_NEW		0	// 服务端事件：有人加入 wparam=hid, lparam=-1
#define QNET_EVENT_LEAVE	1	// 服务端事件：有人离开 wparam=hid, lparam=code
#define QNET_EVENT_DATA		2	// 服务端事件：收到数据 wparam=hid, lparam=protocol

// 取得消息 直接方式，成功返回消息长度，返回-1代表没有消息，-2代表长度错误，
// 如果 event, wparam, lparam 任意为 NULL 则返回消息长度
// event会是：QUICKNET_EVENT_NEW/LEAVE/DATA
QNETAPI int qnet_server_read(QNetServer server, int *event, 
	unsigned long *wparam, unsigned long *lparam, void *data, int size);


// 设置超时
// idle是空闲超时，-1为不设置，0为取消空闲超时，>0为毫秒时间，超过这个时间收不到包的客户端断开
// connect连接超时，-1为不设置，0为取消连接超时，>0为毫秒时间，超过这个时间没连接成功的客户端断开
QNETAPI void qnet_server_timeout(QNetServer server, int idle, int connect);


// 设置内部工作始终，默认是 100ms，改短后有助于提高server网络响应，但耗费cpu
QNETAPI void qnet_server_interval(QNetServer server, int interval);


// 配置网络
QNETAPI int qnet_server_option(QNetServer server, unsigned long hid, const char *options);
QNETAPI int qnet_server_get_option(QNetServer server, unsigned long hid, int option);


// 取得连接状态：0关闭，1连接1，2连接2，3连接等待，4建立连接，5关闭等待。-1为错误 hid
QNETAPI int qnet_server_get_state(const QNetServer server, unsigned long hid);


// 广播一群人，返回成功个数，参数为 unsigned long 列表，默认limit为-1
QNETAPI int qnet_server_groupcast1(QNetServer server, unsigned long *hid, int count,
	int protocol, const void *data, int size, int limit);


// 广播一群人，返回成功个数，参数为 unsigned int 列表，默认limit为-1
QNETAPI int qnet_server_groupcast2(QNetServer server, unsigned int *hid, int count,
	int protocol, const void *data, int size, int limit);


// 广播所有人，返回成功个数
QNETAPI int qnet_server_broadcast(QNetServer server, int protocol, const void *data,
	int size, int limit);

// 设置全局掩码：
QNETAPI void qnet_server_setgmask(QNetServer server, unsigned int mask);

// 取得待发送数据
QNETAPI int qnet_server_get_pending(QNetServer server, unsigned long hid, int what);


//=====================================================================
// 客户端接口
//=====================================================================

// 新建客户端
QNETAPI QNetClient qnet_client_new(void);

// 删除客户端
QNETAPI void qnet_client_delete(QNetClient client);

// 连接远程服务端
QNETAPI int qnet_client_connect(QNetClient client, const char *ip, int port);

//设置Cuteid,为pingrouter 统计信息用
QNETAPI void qnet_client_setcuteid(QNetClient client, unsigned int id);
QNETAPI unsigned int qnet_client_getcuteid(QNetClient client);

//http://192.168.35.247:8866/qnet/get_node_list?key=
QNETAPI void qnet_client_setcgihost(QNetClient client, const char* ip, int port);
QNETAPI void qnet_client_setpingtimes(QNetClient client, int ntimes);
QNETAPI void qnet_client_setpingtimeout(QNetClient client, int nMiliSeconds);
QNETAPI void qnet_client_setpingsleep(QNetClient client, int nMiliSeconds);



//开始一次router only once;
QNETAPI void qnet_client_startrouter(QNetClient client);


// 关闭连接
QNETAPI void qnet_client_close(QNetClient client);


// 发送数据
QNETAPI int qnet_client_send(QNetClient client, int protocol, 
	const void *data, int size);


// 发送数据并限制大小，
// 如果 limit > 0，并且该连接的发送缓存还有 >=limit个待发数据包，则放弃本次发送
QNETAPI int qnet_client_send_limit(QNetClient client, int protocol, 
	const void *data, int size, int limit);


// 接收数据，返回数据长度，如果没有数据则返回-1，缓存长度不够返回-2
QNETAPI int qnet_client_recv(QNetClient client, int *protocol, 
	void *data, int size);


// 更新状态，需要持续调用，建议 10ms一次
QNETAPI void qnet_client_update(QNetClient client);


// 设置保活
QNETAPI void qnet_client_keepalive(QNetClient client, int interval);


// 配置连接
QNETAPI int qnet_client_option(QNetClient client, const char *options);

QNETAPI int qnet_client_get_option(QNetClient client, int option);


// 是否连接：0为没有连接，1为连接了
QNETAPI int qnet_client_is_connected(const QNetClient client);


// 取得状态：0关闭，1连接1，2连接2，3连接等待，4建立连接，5关闭等待。
QNETAPI int qnet_client_get_state(const QNetClient client);

// 取得 rtt
QNETAPI int qnet_client_get_rtt(const QNetClient client);


// 设置超时
QNETAPI void qnet_client_set_timeout(const QNetClient client, int idle, int connect);


// 获取拉包层统计信息
QNETAPI void qnet_client_get_nack_stat_info(QNetClient client, int* packets, int *pull,
	int* pullpkts, int *lost, int *pulltimeout, int *skip, int *totalskippkt);

// 切换目标 IP:PORT 地址
QNETAPI int qnet_client_switch_target(QNetClient client, const char *ip, int port);

// 取得目标 IP:PORT 地址
QNETAPI int qnet_client_get_target(QNetClient client, char *ip, int *port);

// 设置全局掩码：
QNETAPI void qnet_client_setgmask(QNetClient client, unsigned int mask);

// 取得待发送数据
QNETAPI int qnet_client_get_pending(QNetClient client, int what);

// 收发流量统计
QNETAPI void qnet_client_statistic(QNetClient client, struct QNetStatistic *stat);


//=====================================================================
// 日志接口
//=====================================================================
typedef void (*qnet_trace_proc)(const char *text, void *user);

// 设置日志输出函数
QNETAPI void qnet_trace_setout(qnet_trace_proc proc, void *user);

// 打开文件
QNETAPI void qnet_trace_open(const char *prefix, int usestdout);

// 关闭文件
QNETAPI void qnet_trace_close(void);

// 设置掩码
QNETAPI void qnet_trace_setmask(int mask);

// 允许掩码
QNETAPI void qnet_trace_enable(int mask);

// 禁止掩码
QNETAPI void qnet_trace_disable(int mask);

// 设置日志颜色
QNETAPI int qnet_trace_color(int color);

// 输出日志
QNETAPI void qnet_trace_out(int mask, const char *text);

// 检测是否合法
QNETAPI int qnet_trace_available(int mask);



//=====================================================================
// 调试接口
//=====================================================================
QNETAPI int qnet_debug_page_count(void);
QNETAPI int qnet_debug_disable_kmem(void);



//=====================================================================
// TCP非阻塞接口
//=====================================================================
typedef void AsyncSock;
typedef void AsyncCore;


QNETAPI AsyncSock* qnet_tcp_new(void);

QNETAPI void qnet_tcp_delete(AsyncSock *sock);

QNETAPI int qnet_tcp_connect(AsyncSock *sock, const char *ip, int port, int head);

QNETAPI int qnet_tcp_assign(AsyncSock *sock, int fd, int head);

QNETAPI void qnet_tcp_close(AsyncSock *sock);


QNETAPI int qnet_tcp_state(const AsyncSock *sock);

QNETAPI int qnet_tcp_fd(const AsyncSock *sock);

QNETAPI long qnet_tcp_remain(const AsyncSock *sock);



// send data
QNETAPI long qnet_tcp_send(AsyncSock *sock, const void *ptr, long size, int mask);

// recv vector: returns packet size, -1 for not enough data, -2 for 
// buffer size too small, -3 for packet size error, -4 for size over limit,
// returns packet size if ptr equals NULL.
QNETAPI long qnet_tcp_recv(AsyncSock *sock, void *ptr, int size);


// send vector
QNETAPI long qnet_tcp_send_vector(AsyncSock *sock, const void *vecptr[],
	const long veclen[], int count, int mask);

// recv vector: returns packet size, -1 for not enough data, -2 for 
// buffer size too small, -3 for packet size error, -4 for size over limit,
// returns packet size if vecptr equals NULL.
QNETAPI long qnet_tcp_recv_vector(AsyncSock *sock, void *vecptr[], 
	const long veclen[], int count);


// update
QNETAPI int qnet_tcp_update(AsyncSock *sock, int what);

// process
QNETAPI void qnet_tcp_process(AsyncSock *sock);


// set send cryption key
QNETAPI void qnet_tcp_rc4_set_skey(AsyncSock *sock, const unsigned char *key, 
	int keylen);

// set recv cryption key
QNETAPI void qnet_tcp_rc4_set_rkey(AsyncSock *sock, const unsigned char *key, 
	int keylen);

// set nodelay
QNETAPI int qnet_tcp_nodelay(AsyncSock *sock, int nodelay);

// set buf size
QNETAPI int qnet_tcp_sys_buffer(AsyncSock *sock, long rcvbuf, long sndbuf);

// set keepalive
QNETAPI int qnet_tcp_keepalive(AsyncSock *sock, int keepcnt, int idle, int intvl);



//---------------------------------------------------------------------
// TCP 异步事件管理器
//---------------------------------------------------------------------
#define ASYNCCORE_EVT_NEW		0	// new: (hid, tag)
#define ASYNCCORE_EVT_LEAVE		1	// leave: (hid, tag)
#define ASYNCCORE_EVT_ESTAB		2	// estab: (hid, tag)
#define ASYNCCORE_EVT_DATA		3	// data: (hid, tag)

#define ASYNCCORE_NODE_IN			1		// accepted node
#define ASYNCCORE_NODE_OUT			2		// connected out node
#define ASYNCCORE_NODE_LISTEN4		3		// ipv4 listener
#define ASYNCCORE_NODE_LISTEN6		4		// ipv6 listener

// Remote IP Validator: returns 1 to accept it, 0 to reject
typedef int (*AsyncValidator)(const void *remote, int len,
	AsyncCore *core, long listenhid, void *user);

// new AsyncCore object
QNETAPI AsyncCore* qnet_async_new(void);

// delete async core
QNETAPI void qnet_async_delete(AsyncCore *core);


// wait for events for millisec ms. and process events, 
// if millisec equals zero, no wait.
QNETAPI void qnet_async_wait(AsyncCore *core, unsigned long millisec);

// wake-up from qnet_async_wait
QNETAPI void qnet_async_notify(AsyncCore *core);

// read events, returns data length of the message, 
// and returns -1 for no event, -2 for buffer size too small,
QNETAPI long qnet_async_read(AsyncCore *core, int *event, long *wparam,
	long *lparam, void *data, long size);


// send data to given hid
QNETAPI long qnet_async_send(AsyncCore *core, long hid, const void *ptr, long len);

// close given hid
QNETAPI int qnet_async_close(AsyncCore *core, long hid, int code);

// send vector
QNETAPI long qnet_async_send_vector(AsyncCore *core, long hid, const void *vecptr[],
	const long veclen[], int count, int mask);


// send data with mask
QNETAPI long qnet_async_send_mask(AsyncCore *core, long hid, const void *ptr, long len,
	int mask);


// new connection to the target address, returns hid
QNETAPI long qnet_async_new_connect(AsyncCore *core, const char *ip, int port, int header);

// new listener, returns hid
QNETAPI long qnet_async_new_listen(AsyncCore *core, const char *ip, int port, int header);

// new assign, returns hid
QNETAPI long qnet_async_new_assign(AsyncCore *core, int fd, int header, int check_estab);



// get node mode: ASYNCCORE_NODE_IN/OUT/LISTEN4/LISTEN6
QNETAPI int qnet_async_get_mode(const AsyncCore *core, long hid);

// returns connection tag, -1 for hid not exist
QNETAPI long qnet_async_get_tag(const AsyncCore *core, long hid);

// set connection tag
QNETAPI void qnet_async_set_tag(AsyncCore *core, long hid, long tag);

// get send queue size
QNETAPI long qnet_async_remain(const AsyncCore *core, long hid);

// set default buffer limit and max packet size
QNETAPI void qnet_async_limit(AsyncCore *core, long limited, long maxsize);



// get first node
QNETAPI long qnet_async_node_head(const AsyncCore *core);

// get next node
QNETAPI long qnet_async_node_next(const AsyncCore *core, long hid);

// get prev node
QNETAPI long qnet_async_node_prev(const AsyncCore *core, long hid);


#define ASYNCCORE_OPTION_NODELAY		1
#define ASYNCCORE_OPTION_REUSEADDR		2
#define ASYNCCORE_OPTION_KEEPALIVE		3
#define ASYNCCORE_OPTION_SYSSNDBUF		4
#define ASYNCCORE_OPTION_SYSRCVBUF		5
#define ASYNCCORE_OPTION_LIMITED		6
#define ASYNCCORE_OPTION_MAXSIZE		7

// set connection socket option
QNETAPI int qnet_async_option(AsyncCore *core, long hid, int opt, long value);

// get connection socket status
QNETAPI int qnet_async_status(AsyncCore *core, long hid, int opt);

// set connection rc4 send key
QNETAPI int qnet_async_rc4_set_skey(AsyncCore *core, long hid, 
	const unsigned char *key, int keylen);

// set connection rc4 recv key
QNETAPI int qnet_async_rc4_set_rkey(AsyncCore *core, long hid,
	const unsigned char *key, int keylen);

// set remote ip validator
QNETAPI void qnet_async_firewall(AsyncCore *core, AsyncValidator v, void *user);

// set timeout
QNETAPI void qnet_async_timeout(AsyncCore *core, long seconds);

// get sockname
QNETAPI int qnet_async_sockname(const AsyncCore *core, long hid, char *out);

// get peername
QNETAPI int qnet_async_peername(const AsyncCore *core, long hid, char *out);

// disable read poll event
QNETAPI int qnet_async_disable(AsyncCore *core, long hid, int value);


// get number of connections
QNETAPI int qnet_async_nfds(const AsyncCore *core);

// quick post
QNETAPI int qnet_async_post(AsyncCore *core, long wparam, long lparam, const char *data, long size);


//=====================================================================
// AsyncNotify
//=====================================================================
typedef void AsyncNotify;

// create object
QNETAPI AsyncNotify* qnet_notify_new(int serverid);

// delete object
QNETAPI void qnet_notify_delete(AsyncNotify *notify);


#define QNET_NOTIFY_EVT_DATA			1	//  (wp=sid, lp=cmd)
#define QNET_NOTIFY_EVT_NEW_IN			2	//  (wp=sid, lp=hid)
#define QNET_NOTIFY_EVT_NEW_OUT		4	//  (wp=sid, lp=hid)
#define QNET_NOTIFY_EVT_CLOSED_IN		8	//  (wp=sid, lp=hid)
#define QNET_NOTIFY_EVT_CLOSED_OUT		16	//  (wp=sid, lp=hid)
#define QNET_NOTIFY_EVT_ERROR			32	//  (wp=sid, lp=why)
#define QNET_NOTIFY_EVT_CORE			64

// wait events
QNETAPI void qnet_notify_wait(AsyncNotify *notify, unsigned long millisec);

// wake-up from waiting
QNETAPI void qnet_notify_wake(AsyncNotify *notify);

// read events, returns data length of the message, 
// and returns -1 for no event, -2 for buffer size too small,
// returns data size when data equals NULL.
QNETAPI long qnet_notify_read(AsyncNotify *notify, int *event, long *wparam,
	long *lparam, void *data, long maxsize);


// new listen: return id(-1 error, -2 port conflict), flag&1(reuse)
QNETAPI long qnet_notify_listen(AsyncNotify *notify, const char *addr, int port, int flag);

// remove listening port
QNETAPI int qnet_notify_remove(AsyncNotify *notify, long listenid, int code);

// setup self server id
QNETAPI void qnet_notify_change(AsyncNotify *notify, int new_server_id);


// send message to server
QNETAPI int qnet_notify_send(AsyncNotify *notify, int sid, short cmd, 
	const void *data, long size);

// close server connection
QNETAPI int qnet_notify_close(AsyncNotify *notify, int sid, int mode, int code);

// get listening port
QNETAPI int qnet_notify_get_port(AsyncNotify *notify, long listenid);


// clear ip allow table
QNETAPI void qnet_notify_allow_clear(AsyncNotify *notify);

// add or update ip in allow table
QNETAPI void qnet_notify_allow_add(AsyncNotify *notify, const char *ip);

// remove ip from table
QNETAPI void qnet_notify_allow_del(AsyncNotify *notify, const char *ip);

// ip table enable: enable is 0(disable allow table) otherwise(enable)
QNETAPI void qnet_notify_allow_enable(AsyncNotify *notify, int enable);


// add or update a sid into sid2addr
QNETAPI void qnet_notify_sid_add(AsyncNotify *notify, int sid, const char *ip, int port);

// add or update a sid into sid2addr
QNETAPI void qnet_notify_sid_del(AsyncNotify *notify, int sid);

// list sids into an array
QNETAPI int qnet_notify_sid_list(AsyncNotify *notify, int *sids, int maxsize);

// clear sid list
QNETAPI void qnet_notify_sid_clear(AsyncNotify *notify);


#define QNET_NOTIFY_OPT_PROFILE				0
#define QNET_NOTIFY_OPT_TIMEOUT_IDLE		1
#define QNET_NOTIFY_OPT_TIMEOUT_PING		2
#define QNET_NOTIFY_OPT_SOCK_KEEPALIVE		3
#define QNET_NOTIFY_OPT_SND_BUFSIZE			4
#define QNET_NOTIFY_OPT_RCV_BUFSIZE			5
#define QNET_NOTIFY_OPT_BUFFER_LIMIT		6
#define QNET_NOTIFY_OPT_SIGN_TIMEOUT		7
#define QNET_NOTIFY_OPT_RETRY_TIMEOUT		8
#define QNET_NOTIFY_OPT_NET_TIMEOUT			9
#define QNET_NOTIFY_OPT_EVT_MASK			10
#define QNET_NOTIFY_OPT_LOG_MASK			11
#define QNET_NOTIFY_OPT_GET_PING			12
#define QNET_NOTIFY_OPT_GET_OUT_COUNT		13
#define QNET_NOTIFY_OPT_GET_IN_COUNT		14

#define QNET_NOTIFY_LOG_INFO		1
#define QNET_NOTIFY_LOG_REJECT		2
#define QNET_NOTIFY_LOG_ERROR		4
#define QNET_NOTIFY_LOG_WARNING		8

// config
QNETAPI int qnet_notify_option(AsyncNotify *notify, int type, long value);

// set login token
QNETAPI void qnet_notify_token(AsyncNotify *notify, const char *token, int size);

// set logging
QNETAPI void qnet_notify_trace(AsyncNotify *notify, const char *prefix, int STDOUT, int color);


#ifdef __cplusplus
}
#endif


//=====================================================================
// C++接口
//=====================================================================
#ifdef __cplusplus

namespace QuickNet {

class Client
{
public:
    Client() { client = qnet_client_new(); }

	virtual ~Client() { if (client) qnet_client_delete(client); }

	// 连接服务端
	int connect(const char *ip, int port) { return qnet_client_connect(client, ip, port); }
    //设置Cuteid,为pingrouter 统计信息用
    void set_cuteid(unsigned int id)  { qnet_client_setcuteid(client, id); }
    unsigned int get_cuteid() { return  qnet_client_getcuteid(client); }

    //http://192.168.35.247:8866/qnet/get_node_list?key=
    void set_cgi_host(const char* pszIP, int port) { qnet_client_setcgihost(client, pszIP, port); }

    void set_ping_param(int nPingTimes, int nTimeoutMS, int nSleepMS) 
    { 
        qnet_client_setpingtimes(client, nPingTimes); 
        qnet_client_setpingtimeout(client, nTimeoutMS); //million seconds;
        qnet_client_setpingsleep(client, nSleepMS);
    }

    //开始一次router only once;
    void start_router() { qnet_client_startrouter(client); }
	// 关闭连接
	void close() { qnet_client_close(client); }

	// 发送数据
	// 如果 limit > 0，并且该连接的发送缓存还有 >=limit个待发数据包，则放弃本次发送
	int send(int protocol, const void *data, int size, int limit = -1) {
		return qnet_client_send_limit(client, protocol, data, size, limit); 
	}

	// 接收数据，返回数据长度，如果没有数据则返回-1，缓存长度不够返回-2
	int recv(int *protocol, void *data, int size) {
		return qnet_client_recv(client, protocol, data, size);
	}

 	// 更新状态，需要持续调用，建议 10ms一次
	void update() { qnet_client_update(client); }

	// 设置保活周期
	void keepalive(int interval) { qnet_client_keepalive(client, interval); }

	// 配置客户端，传入参数字符串
	int option(const char *options) { return qnet_client_option(client, options); }

	int getoption(int option) { return qnet_client_get_option(client, option); }

	// 返回是否连接，1为连接，0为没有连接
	int isconnected() const { return qnet_client_is_connected(client); }

	// 取得状态：0关闭，1连接1，2连接2，3连接等待，4建立连接，5关闭等待。
	int state() const { return qnet_client_get_state(client); }

	// 取得 Rtt
	int getrtt() const { return qnet_client_get_rtt(client); }

	/*
	* @packets  总报文数
	* @pull		被拉过包的报文数
	* @pullpkts	实际拉包消息数，当前拉包策略下(仅仅参考)
	* @lost		实际丢包数
	* @pulltimeout 拉包后超时包个数（仅仅参考）
	* @skip		连续包忽略拉包次数
	* @totalskippkt 连续包忽略拉包总个数
	*/
	void getnackstatinfo(int* packets, int *pull, int* pullpkts, int *lost, int *pulltimeout, int *skip, int *totalskippkt) 
		{qnet_client_get_nack_stat_info(client, packets, pull, pullpkts, lost, pulltimeout, skip, totalskippkt);}

	// 设置目标地址：用于IP映射时设置中间节点（具有IP镜像功能的服务端）
	// 成功返回0，失败返回-1
	int target_switch(const char *ip, int port) { return qnet_client_switch_target(client, ip, port); }

	// 取得目标地址：返回当前的目标地址
	int target_get(char *ip, int *port) { return qnet_client_get_target(client, ip, port); }

	// 设置全局掩码
	void set_gmask(unsigned int mask) { qnet_client_setgmask(client, mask); }

	// 设置超时
	void set_timeout(int idle = -1, int connect = -1) { qnet_client_set_timeout(client, idle, connect); }

	// 获取待发数据
	int get_pending(int what) {return qnet_client_get_pending(client, what);}

	// 统计信息
	void statistic(QNetStatistic &s) { qnet_client_statistic(client, &s); }

protected:
	QNetClient client;
};


//=====================================================================
// 客户端纯净接口
//=====================================================================
struct QuickSocket
{
	// 删除自己
	virtual void Release() = 0;

	// 建立连接
	virtual int connect(const char *ip, int port) = 0;

	// 关闭连接
	virtual void close() = 0;

	// 发送数据
	// 如果 limit > 0，并且该连接的发送缓存还有 >=limit个待发数据包，则放弃本次发送
	virtual int send(int protocol, const void *data, int size, int limit = -1) = 0;

	// 接收数据，返回数据长度，如果没有数据则返回-1，缓存长度不够返回-2
	virtual int recv(int *protocol, void *data, int size) = 0;

	// 更新协议时钟，需要持续调用，建议 10ms一次，
	virtual void update() = 0;

	// 设置保活周期
	virtual void keepalive(int interval) = 0;

	// 配置客户端，传入参数字符串
	virtual int option(const char *options) = 0;

	// 取得配置
	virtual int getoption(int option) = 0;

	// 返回是否连接，1为连接，0为没有连接
	virtual int isconnected() const = 0;

	// 取得状态：0关闭，1连接1，2连接2，3连接等待，4建立连接，5关闭等待。
	virtual int state() const = 0;

	// 取得 Rtt
	virtual int getrtt() const = 0; 

	/*
	* @packets  总报文数
	* @pull		被拉过包的报文数
	* @pullpkts	实际拉包消息数，当前拉包策略下(仅仅参考)
	* @lost		实际丢包数
	* @pulltimeout 拉包后超时包个数（仅仅参考）
	* @skip		连续包忽略拉包次数
	* @totalskippkt 连续包忽略拉包总个数
	*/
	virtual void getnackstatinfo(int* packets, int *pull, int* pullpkts, 
		int *lost, int *pulltimeout, int *skip, int *totalskippkt) = 0;

	// 设置目标地址：用于IP映射时设置中间节点（具有IP镜像功能的服务端）
	// 成功返回0，失败返回-1
	virtual int target_switch(const char *ip, int port) = 0;

	// 取得目标地址：返回当前的目标地址
	virtual int target_get(char *ip, int *port) = 0;

	// 设置全局掩码
	virtual void set_gmask(unsigned int mask) = 0;
};

};

QNETAPI QuickNet::QuickSocket* QuickSocket_Create(void);


#endif

#endif


