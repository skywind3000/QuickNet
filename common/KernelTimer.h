//=====================================================================
//
// KernelTimer.h - 时钟轮封装
//
// Last Modified: 2019/06/18 10:30:34
//
//=====================================================================
#ifndef _KERNEL_TIMER_H_
#define _KERNEL_TIMER_H_

#include "../system/itimer.h"
#include "../system/system.h"

#include <vector>
#include <unordered_map>


NAMESPACE_BEGIN(AsyncNet);

//---------------------------------------------------------------------
// KernelTimer
//---------------------------------------------------------------------
class KernelTimer
{
public:
	virtual ~KernelTimer();

	typedef void (*Callback)(void *obj, int id, int tag);

	KernelTimer(Callback cb, void *obj, int reserved = 1024);

public:
	// 传入当前毫秒级时钟，32位整数，支持时钟回环
	void run(uint32_t current);

	// 分配时钟
	int create(int tag);

	// kill
	int kill(int id);

	// start
	int start(int id, unsigned int period, int repeat);

	// stop
	int stop(int id);

	// tag
	int set_tag(int id, int tag);

	// 检测是否存在
	bool check_exists(int id) const;

	// 检测是否活动
	bool check_started(int id) const;

protected:
	KernelTimer(const KernelTimer &);

	Callback _cb_fn;
	void *_cb_obj;

	static void _timer_callback(void *data, void *user);
	void handle_timer(void *node);

	int _node_alloc();
	int _node_delete(int id);

	struct Node { itimer_evt evt; int id; int tag; int repeat; };
	Node *_node_find(int id);
	const Node *_node_find(int id) const;

protected:
	itimer_mgr _timer_mgr;
	int _index;
	int _reserved;
	typedef std::unordered_map<int, Node*> TimerMap;
	TimerMap _timer_map;
	std::vector<Node*> _static_nodes;
	std::vector<int> _pending_remove;
};


NAMESPACE_END(AsyncNet);


#endif


