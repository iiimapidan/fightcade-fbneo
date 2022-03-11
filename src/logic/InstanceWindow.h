#ifndef _COMMON_INSTANCEWINDOW_UTILS_H_
#define _COMMON_INSTANCEWINDOW_UTILS_H_

#include <Windows.h>
#include <functional>
#include <map>
#include <StlLock.h>
#include "Singleton.h"

typedef std::function<void()> InstanceWindowCallback;
typedef std::function<LRESULT(WPARAM wParam, LPARAM lParam)> WindowCallback;

class InstanceWindow {
public:
	InstanceWindow();
	bool Init(LPCWSTR wnd_class_name);
	void Uninit();
	HWND GetHWND() const { return _wnd; }

	// �ӳ�ִ��һ�����̻߳ص���delay_ms���ӳٺ��룬����Ϊ0
	unsigned PostCallback(const InstanceWindowCallback& cb, unsigned delay_ms = 0);

	// ��ʱ�����̻߳ص�������timer_id��delay_ms���ӳٺ��룬����Ϊ0
	unsigned AddTimerCallback(const InstanceWindowCallback& cb, unsigned delay_ms = 0);

	// PostCallback | AddTimerCallback ��ִ��ǰ�������ô˷�������ȡ��
	bool RemoveTimerCallback(unsigned timer_id);

	// ע����Ϣ�ص���һ����Ϣֻ��ע��һ���ص�
	void RegisterWindowCallback(unsigned msg, const WindowCallback& cb);
	void UnregisterWindowCallback(unsigned msg);

	static LRESULT CALLBACK InstanceProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	void ExecTimer(unsigned timer_id);
	LRESULT ExecMessage(UINT message, WPARAM wp, LPARAM lp);

private:
	std::map<unsigned, InstanceWindowCallback> _wnd_delay_call_map;
	std::map<unsigned, InstanceWindowCallback> _wnd_timer_call_map;
	std::map<unsigned, WindowCallback> _wnd_msg_call_map;
	CCritSec _lock;
	HWND _wnd;
	unsigned _timer_id;
};

typedef SingletonClass<InstanceWindow> InstanceWindowManager;

#define InstanceWindowCallback(callback) CommonFunc::InstanceWindowManager::GetInstance()->PostCallback(callback, 0)
#define InstanceWindowPostCallback(callback, time) CommonFunc::InstanceWindowManager::GetInstance()->PostCallback(callback, time)
#define InstanceWindowAddTimer(callback, time) CommonFunc::InstanceWindowManager::GetInstance()->AddTimerCallback(callback, time)
#define InstanceWindowRemove(id) CommonFunc::InstanceWindowManager::GetInstance()->RemoveTimerCallback(id)

#define InstanceWindowRegisterMessage(message, callback) CommonFunc::InstanceWindowManager::GetInstance()->RegisterWindowCallback(message, callback)
#define InstanceWindowUnregisterMessage(message) CommonFunc::InstanceWindowManager::GetInstance()->UnregisterWindowCallback(message)

#endif
