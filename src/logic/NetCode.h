#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <map>
#include "Singleton.h"
#include "hv/tcpclient.h"
#include "hv/requests.h"
#include "pb/Message.pb.h"



#define WM_CREATE_CONSOLE		(WM_USER + 1)
#define WM_CONNECTED_SERVER		(WM_USER + 2)
#define WM_CREATE_OR_JOIN_ROOM	(WM_USER + 3)
#define WM_CREATED_ROOM			(WM_USER + 4)
#define WM_JOINED_ROOM			(WM_USER + 5)
#define WM_WAIT_GAME_START		(WM_USER + 6)
#define WM_GAME_STARTED			(WM_USER + 7)
#define WM_AUTO_MATCH			(WM_USER + 8)
#define WM_PRINT_LOG			(WM_USER + 9)


#define WM_RUN_NET_GAME			(WM_USER + 1000 + 1)
#define WM_RECEIVE_REMOTE_FRAME (WM_USER + 1000 + 2)


typedef std::vector<char> INPUT_DATA;


typedef struct _InputData {
	int frameId;
	std::vector<char> data;
	std::string uuid;
	std::string inputName;
}InputData;


typedef struct _SavedFrame {
	unsigned char* buf;
	int bufCounts;
	int frameId; 
	int checksum;
}SavedFrame;

class IPlayEvent {
public:
	virtual void onStartGame() = 0;
	virtual void onReceiveRemoteFrame(const InputData& input) = 0;
};

typedef struct _IGameCallback {
	bool(__cdecl* begin_game)(char* game);
	void(__cdecl* free_buffer)(void* buffer);
	bool(__cdecl* load_game_state)(unsigned char* buffer, int len);
	bool(__cdecl* save_game_state)(unsigned char** buffer, int* len, int* checksum, int frame);
	bool(__cdecl* advance_frame)(int flags);
}IGameCallback;

class NetCode {
public:
	NetCode();
	~NetCode();

	bool init();
	void setPlayEvent(IPlayEvent *event);
	void setGameCallback(IGameCallback* gameCallback);

	void sendGameReady();
	void waitGameStarted();

	void increaseFrame();

	bool getNetInput(void* values, int size, int players, bool syncOnly, std::string inputName);

	void receiveRemoteFrame(const InputData& remoteFrame);
	void checkRollback();

	void printLog(const std::wstring& method, const std::wstring& log);

private:
	bool connectServer();
	void createConsole();
	static DWORD WINAPI consoleThread(void* pParam);

	void createRoom();
	void joinRoom(::google::protobuf::uint32 roomId);
	void autoMatch();
	void sendLog(const std::wstring& method, const std::wstring& log);

	void sendLocalInput(const InputData& input);
	void fetchFrame(int id, void* values);
	bool addLocalInput(char* values, int size, int players, std::string inputName);
	void addRemoteInput();

	void saveCurrentFrameState();

	int cmpInputData(const InputData& input1, const InputData& input2);

	std::wstring formatFrameData(const InputData& inputFrame);

	std::string generate();

private:
	hv::TcpClient _client;

	DWORD _threadId = 0;
	HANDLE _eventThreadStarted = NULL;

	IPlayEvent* _playEvent = NULL;

	::google::protobuf::uint32 _roomId = 0;
	::google::protobuf::uint32 _playId = 0;

	int _frameId = -1;

	HANDLE _eventGameStarted = NULL;

	std::map<int, SavedFrame> _savedFrame;

	std::map<int, int> _savedFrameValue;

	std::map<int, InputData> _localInputMap;
	std::map<int, InputData> _remoteInputMap;

	std::queue<InputData> _remoteInputNetCacheQueue;

	InputData _predictFrame;
	int _firstPredictFrameId = -1;
	bool _is_rollback = false;

	IGameCallback* _gameCallback = NULL;
};

typedef SingletonClass<NetCode> NetCodeManager;


bool __cdecl netcode_begin_game_callback(char* name);
void __cdecl netcode_free_buffer_callback(void* buffer);
bool __cdecl netcode_load_game_state_callback(unsigned char* buffer, int len);
bool __cdecl netcode_save_game_state_callback(unsigned char** buffer, int* len, int* checksum, int frame);
bool __cdecl netcode_advance_frame_callback(int flags);