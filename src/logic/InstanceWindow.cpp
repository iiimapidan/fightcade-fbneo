#include "InstanceWindow.h"

LRESULT CALLBACK InstanceWindow::InstanceProc(HWND wnd, UINT message, WPARAM wp, LPARAM lp) {
	LRESULT result = 0;

	switch (message) {
	case WM_TIMER:
	{
		InstanceWindowManager::GetInstance()->ExecTimer((unsigned)wp);
		break;
	}
	default:
	{
		result = InstanceWindowManager::GetInstance()->ExecMessage(message, wp, lp);
		break;
	}
	}

	if (result)
		return result;

	return ::DefWindowProc(wnd, message, wp, lp);
}

HWND __CreateInstanceWindow(LPCWSTR class_name) {
	WNDCLASS wc = {};
	wc.lpszClassName = class_name;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpfnWndProc = &InstanceWindow::InstanceProc;
	RegisterClass(&wc);
	return CreateWindow(class_name, L"", WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, wc.hInstance, NULL);
}

InstanceWindow::InstanceWindow() : _wnd(NULL), _timer_id(1) {
}

bool InstanceWindow::Init(LPCWSTR wnd_class_name) {
	if (_wnd && IsWindow(_wnd))
		return true;
	_wnd = __CreateInstanceWindow(wnd_class_name);
	return (IsWindow(_wnd) == TRUE);
}

void InstanceWindow::Uninit() {
	_lock.Enter();

	if (_wnd) {
		DestroyWindow(_wnd);
		_wnd = NULL;
		_wnd_delay_call_map.clear();
		_wnd_timer_call_map.clear();
		_wnd_msg_call_map.clear();
	}

	_lock.Leave();
}

unsigned InstanceWindow::PostCallback(const InstanceWindowCallback& cb, unsigned delay_ms) {
	unsigned id = 0;
	_lock.Enter();

	if (_wnd) {
		if (0 == _timer_id)
			++_timer_id;
		SetTimer(_wnd, _timer_id, delay_ms, NULL);
		_wnd_delay_call_map[_timer_id] = cb;
		id = _timer_id;
		++_timer_id;
	}

	_lock.Leave();

	return id;
}

unsigned InstanceWindow::AddTimerCallback(const InstanceWindowCallback& cb, unsigned delay_ms /*= 0*/) {
	unsigned id = 0;
	_lock.Enter();

	if (_wnd) {
		if (0 == _timer_id)
			++_timer_id;
		SetTimer(_wnd, _timer_id, delay_ms, NULL);
		_wnd_timer_call_map[_timer_id] = cb;
		id = _timer_id;
		++_timer_id;
	}

	_lock.Leave();

	return id;
}

bool InstanceWindow::RemoveTimerCallback(unsigned timer_id) {
	bool result = false;

	_lock.Enter();

	auto it = _wnd_timer_call_map.find(timer_id);

	if (it != _wnd_timer_call_map.end()) {
		_wnd_timer_call_map.erase(it);
		result = true;
	} else {
		it = _wnd_delay_call_map.find(timer_id);
		if (it != _wnd_delay_call_map.end()) {
			_wnd_delay_call_map.erase(it);
			result = true;
		}
	}

	_lock.Leave();

	return result;
}

void InstanceWindow::RegisterWindowCallback(unsigned msg, const WindowCallback& cb) {
	_lock.Enter();
	_wnd_msg_call_map[msg] = cb;
	_lock.Leave();
}

void InstanceWindow::UnregisterWindowCallback(unsigned msg) {
	_lock.Enter();
	auto it = _wnd_msg_call_map.find(msg);
	if (it != _wnd_msg_call_map.end())
		_wnd_msg_call_map.erase(it);
	_lock.Leave();
}

void InstanceWindow::ExecTimer(unsigned timer_id) {
	InstanceWindowCallback cb;

	_lock.Enter();

	auto it = _wnd_timer_call_map.find(timer_id);

	if (it != _wnd_timer_call_map.end()) {
		cb = it->second;
	} else {
		it = _wnd_delay_call_map.find(timer_id);

		if (it != _wnd_delay_call_map.end()) {
			KillTimer(_wnd, timer_id);
			cb = it->second;
			_wnd_delay_call_map.erase(it);
		}
	}

	_lock.Leave();

	if (cb)
		cb();
}

LRESULT InstanceWindow::ExecMessage(UINT message, WPARAM wp, LPARAM lp) {
	LRESULT result = 0;
	WindowCallback cb;

	_lock.Enter();

	auto it = _wnd_msg_call_map.find(message);

	if (it != _wnd_msg_call_map.end()) {
		cb = it->second;
	}

	_lock.Leave();

	if (cb)
		result = cb(wp, lp);

	return result;
}
