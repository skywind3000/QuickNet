//=====================================================================
//
// KernelTimer.cpp - 
//
// Last Modified: 2019/06/18 10:31:13
//
//=====================================================================
#include "KernelTimer.h"
#include <algorithm>

NAMESPACE_BEGIN(AsyncNet);
//---------------------------------------------------------------------
// 
//---------------------------------------------------------------------
KernelTimer::KernelTimer(Callback cb, void *obj, int reserved)
{
	itimer_mgr_init(&_timer_mgr, 0, 1);
	_cb_fn = cb;
	_cb_obj = obj;
	_reserved = reserved;
	_index = reserved;
	_static_nodes.resize(reserved);
	for (int i = 0; i < reserved; i++) {
		Node *node = new Node();
		node->id = i;
		node->tag = -1;
		node->repeat = 0;
		itimer_evt_init(&node->evt, _timer_callback, this, node);
		_static_nodes[i] = node;
	}
}


//---------------------------------------------------------------------
// 
//---------------------------------------------------------------------
KernelTimer::~KernelTimer()
{
	while (!_timer_map.empty()) {
		Node *node = _timer_map.begin()->second;
		_node_delete(node->id);
	}
	_timer_map.clear();
	for (int i = 0; i < (int)_static_nodes.size(); i++) {
		Node *node = _static_nodes[i];
		if (node) {
			itimer_evt_destroy(&node->evt);
			delete node;
		}
		_static_nodes[i] = NULL;
	}
	itimer_mgr_destroy(&_timer_mgr);
}


//---------------------------------------------------------------------
// alloc node
//---------------------------------------------------------------------
int KernelTimer::_node_alloc()
{
	size_t limit = 0x7ffffff0 - _reserved;
	if (_timer_map.size() >= limit) 
		return -1;
	while (1) {
		_index++;
		_index = std::max(_index, _reserved);
		if (_index >= 0x7fffffff) _index = _reserved;
		if (_node_find(_index) == NULL) {
			break;
		}
	}
	Node *node = new Node();
	node->id = _index;
	node->tag = -1;
	node->repeat = 0;
	itimer_evt_init(&node->evt, _timer_callback, this, node);
	_timer_map[_index] = node;
	return _index;
}


//---------------------------------------------------------------------
// delete node
//---------------------------------------------------------------------
int KernelTimer::_node_delete(int id)
{
	if (id >= 0 && id < _reserved) {
		Node *node = _static_nodes[id];
		if (node) {
			itimer_evt_stop(&_timer_mgr, &node->evt);
		}
	}
	else {
		TimerMap::iterator it = _timer_map.find(id);
		if (it == _timer_map.end())
			return -1;
		Node *node = it->second;
		if (node) {
			itimer_evt_destroy(&node->evt);
			node->id = -1;
			delete node;
		}
		it->second = NULL;
		_timer_map.erase(it);
	}
	return 0;
}


//---------------------------------------------------------------------
// find
//---------------------------------------------------------------------
KernelTimer::Node* KernelTimer::_node_find(int id)
{
	if (id >= 0 && id < _reserved) {
		return _static_nodes[id];
	}
	else {
		TimerMap::iterator it = _timer_map.find(id);
		if (it == _timer_map.end())
			return NULL;
		return it->second;
	}
}


//---------------------------------------------------------------------
// find const
//---------------------------------------------------------------------
const KernelTimer::Node* KernelTimer::_node_find(int id) const
{
	if (id >= 0 && id < _reserved) {
		return _static_nodes[id];
	}
	else {
		TimerMap::const_iterator it = _timer_map.find(id);
		if (it == _timer_map.end())
			return NULL;
		return it->second;
	}
}


//---------------------------------------------------------------------
// 运行时钟
//---------------------------------------------------------------------
void KernelTimer::run(uint32_t current)
{
	_pending_remove.resize(0);
	itimer_mgr_run(&_timer_mgr, current);
	for (int i = 0; i < (int)_pending_remove.size(); i++) {
		int id = _pending_remove[i];
		_node_delete(id);
	}
}


//---------------------------------------------------------------------
// static timer callback
//---------------------------------------------------------------------
void KernelTimer::_timer_callback(void *data, void *user)
{
	KernelTimer *self = reinterpret_cast<KernelTimer*>(data);
	self->handle_timer(user);
}


//---------------------------------------------------------------------
// 执行时钟
//---------------------------------------------------------------------
void KernelTimer::handle_timer(void *node)
{
	Node *n = reinterpret_cast<Node*>(node);
	if (_cb_fn) {
		_cb_fn(_cb_obj, n->id, n->tag);
	}
}


//---------------------------------------------------------------------
// 分配时钟
//---------------------------------------------------------------------
int KernelTimer::create(int tag)
{
	int id = _node_alloc();
	if (id < 0) return -1;
	Node *node = _node_find(id);
	assert(node);
	node->tag = tag;
	return id;
}


//---------------------------------------------------------------------
// kill
//---------------------------------------------------------------------
int KernelTimer::kill(int id)
{
	return _node_delete(id);
}


//---------------------------------------------------------------------
// start
//---------------------------------------------------------------------
int KernelTimer::start(int id, unsigned int period, int repeat)
{
	Node *node = _node_find(id);
	if (node == NULL) {
		return -1;
	}
	itimer_evt_start(&_timer_mgr, &node->evt, period, repeat);
	node->repeat = (repeat <= 0)? -1 : repeat;
	return 0;
}


//---------------------------------------------------------------------
// stop
//---------------------------------------------------------------------
int KernelTimer::stop(int id)
{
	Node *node = _node_find(id);
	if (node == NULL) {
		return -1;
	}
	itimer_evt_stop(&_timer_mgr, &node->evt);
	return 0;
}


//---------------------------------------------------------------------
// 
//---------------------------------------------------------------------
int KernelTimer::set_tag(int id, int tag)
{
	Node *node = _node_find(id);
	if (node == NULL) {
		return -1;
	}
	node->tag = tag;
	return 0;
}


//---------------------------------------------------------------------
// 检测是否存在
//---------------------------------------------------------------------
bool KernelTimer::check_exists(int id) const
{
	const Node *node = _node_find(id);
	return (node != NULL)? true : false;
}


//---------------------------------------------------------------------
// 检测是否活动
//---------------------------------------------------------------------
bool KernelTimer::check_started(int id) const
{
	const Node *node = _node_find(id);
	if (node == NULL) return false;
	if (node->evt.mgr == NULL) return false;
	return true;
}



NAMESPACE_END(AsyncNet);



