//=====================================================================
//
// QuickTimer.cpp - 
//
// NOTE:
// for more information, please see the readme file.
//
//=====================================================================
#include "QuickTimer.h"
#include <memory>

namespace System {

//---------------------------------------------------------------------
// init
//---------------------------------------------------------------------
QuickTimerScheduler::QuickTimerScheduler() :
	_timer_mgr(std::make_unique<itimer_mgr>())
{
	itimer_mgr_init(_timer_mgr.get(), 1);
}


//---------------------------------------------------------------------
// destroy
//---------------------------------------------------------------------
QuickTimerScheduler::~QuickTimerScheduler() 
{
	itimer_mgr_destroy(_timer_mgr.get());
}


//---------------------------------------------------------------------
// update timer
//---------------------------------------------------------------------
void QuickTimerScheduler::Update(uint32_t now)
{
	itimer_mgr_run(_timer_mgr.get(), now);
}


//---------------------------------------------------------------------
// returns timer manager
//---------------------------------------------------------------------
itimer_mgr* QuickTimerScheduler::GetTimerManager()
{
	return _timer_mgr.get();
}


//---------------------------------------------------------------------
// init timer
//---------------------------------------------------------------------
QuickTimer::QuickTimer(QuickTimerScheduler *scheduler) :
	_timer_id(std::make_unique<itimer_evt>()),
	_scheduler(scheduler)
{
	itimer_evt_init(_timer_id.get(), TimerCallback, this, NULL);
}


//---------------------------------------------------------------------
// releae timer: remove itself from scheduler
//---------------------------------------------------------------------
QuickTimer::~QuickTimer()
{
	itimer_evt_destroy(_timer_id.get());
}


//---------------------------------------------------------------------
// 
//---------------------------------------------------------------------
void QuickTimer::StartTimer(uint32_t period, int repeat)
{
	itimer_evt_start(_scheduler->GetTimerManager(), _timer_id.get(), period, repeat);
}


//---------------------------------------------------------------------
// 
//---------------------------------------------------------------------
void QuickTimer::StopTimer()
{
	itimer_evt_stop(_scheduler->GetTimerManager(), _timer_id.get());
}


//---------------------------------------------------------------------
// 
//---------------------------------------------------------------------
bool QuickTimer::IsRunning() const
{
	if (_scheduler) {
		return itimer_evt_status(_timer_id.get())? true : false;
	}
	return false;
}


//---------------------------------------------------------------------
// callback
//---------------------------------------------------------------------
void QuickTimer::TimerCallback(void *data, void *user)
{
	if (data) {
		QuickTimer *timer = (QuickTimer*)data;
		if (timer->_callback) {
			timer->_callback(timer, user);
		}
	}
}



//---------------------------------------------------------------------
// bind callback
//---------------------------------------------------------------------
void QuickTimer::Bind(OnTimer func, void *user)
{
	_callback = func;
	_user = user;
}


}



