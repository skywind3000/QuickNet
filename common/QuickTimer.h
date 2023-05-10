//=====================================================================
//
// QuickTimer.h - 
//
// NOTE:
// for more information, please see the readme file.
//
//=====================================================================
#ifndef _QUICK_TIMER_H_
#define _QUICK_TIMER_H_

#include <stdint.h>
#include <functional>
#include <memory>

#include "../system/itimer.h"
#include "../system/system.h"


NAMESPACE_BEGIN(System);

//---------------------------------------------------------------------
// timer scheduler
//---------------------------------------------------------------------
class QuickTimerScheduler
{
public:
	QuickTimerScheduler();
	virtual ~QuickTimerScheduler();

	void Update(uint32_t now);

	itimer_mgr* GetTimerManager();

private:
	std::unique_ptr<itimer_mgr> _timer_mgr;
};


//---------------------------------------------------------------------
// timer
//---------------------------------------------------------------------
class QuickTimer
{
public:
	typedef std::function<void(QuickTimer*, void* user)> OnTimer;

	QuickTimer(QuickTimerScheduler *scheduler);
	virtual ~QuickTimer();

	void Bind(OnTimer func, void *user = NULL);

	void StartTimer(uint32_t period, int repeat);
	void StopTimer();
	bool IsRunning() const;

private:
	static void TimerCallback(void *data, void *user);
	void Callback(void *user);

private:
	std::unique_ptr<itimer_evt> _timer_id;
	OnTimer _callback;
	QuickTimerScheduler *_scheduler;
	void *_user;
};



NAMESPACE_END(System);


#endif



