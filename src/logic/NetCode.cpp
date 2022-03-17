#include "NetCode.h"

#include <stdio.h>
#include <sstream>
#include <string>

#include "bufferptr.h"
#include "InstanceWindow.h"
#include "utils/fmt/format.h"
#include "utils/string/StringConvert.h"
#include "utils/log/loguru.hpp"
#include "state.h"
#include <tchar.h>
#include "burner.h"

#include <stdio.h>
#include <stdint.h>
#include <string>
#include <objbase.h>

#pragma pack(1)
typedef struct _MsgHead {
	uint32_t body_len;
	uint32_t id;
	uint64_t timestamp;
	BYTE* body[0];
}MessageHead;


#pragma pack()



static char gAcbBuffer[16 * 1024 * 1024];
static char* gAcbScanPointer;
static int gAcbChecksum;
const int ggpo_state_header_size = 6 * sizeof(int);

extern int nAcbVersion;
extern int bMediaExit;
extern int nAcbLoadState;

static int QuarkReadAcb(struct BurnArea* pba) {
	memcpy(gAcbScanPointer, pba->Data, pba->nLen);
	gAcbScanPointer += pba->nLen;
	return 0;
}
static int QuarkWriteAcb(struct BurnArea* pba) {
	memcpy(pba->Data, gAcbScanPointer, pba->nLen);
	gAcbScanPointer += pba->nLen;
	return 0;
}

bool __cdecl netcode_begin_game_callback(char* name) {
	WIN32_FIND_DATA fd;
	TCHAR tfilename[MAX_PATH];
	TCHAR tname[MAX_PATH];
	ANSIToTCHAR(name, tname, MAX_PATH);

	// no savestate
	UINT32 i;
	for (i = 0; i < nBurnDrvCount; i++) {
		nBurnDrvActive = i;
		if ((_tcscmp(BurnDrvGetText(DRV_NAME), tname) == 0) && (!(BurnDrvGetFlags() & BDF_BOARDROM))) {
			MediaInit();
			DrvInit(i, true);
			// load game detector
			DetectorLoad(name, false, time(NULL));
			return 1;
		}
	}

	return 0;
}

void __cdecl netcode_free_buffer_callback(void* buffer) {
	free(buffer);
}

bool __cdecl netcode_load_game_state_callback(unsigned char* buffer, int len) {
	int* data = (int*)buffer;
	if (data[0] == 'GGPO') {
		int headersize = data[1];
		int num = headersize / sizeof(int);
		// version
		nAcbVersion = data[2];
		int state = (data[3]) & 0xff;
		int score1 = (data[3] >> 8) & 0xff;
		int score2 = (data[3] >> 16) & 0xff;
		int ranked = (data[3] >> 24) & 0xff;
		int start1 = 0;
		int start2 = 0;
		if (num > 4) {
			start1 = (data[4]) & 0xff;
			start2 = (data[4] >> 8) & 0xff;
		}
		buffer += headersize;
	}
	gAcbScanPointer = (char*)buffer;
	BurnAcb = QuarkWriteAcb;
	nAcbLoadState = false;
	BurnAreaScan(ACB_FULLSCANL | ACB_WRITE, NULL);
	nAcbLoadState = 0;
	nAcbVersion = nBurnVer;
	return true;
}

bool __cdecl netcode_save_game_state_callback(unsigned char** buffer, int* len, int* checksum, int frame) {
	int payloadsize;

	gAcbChecksum = 0;
	gAcbScanPointer = gAcbBuffer;
	BurnAcb = QuarkReadAcb;
	BurnAreaScan(ACB_FULLSCANL | ACB_READ, NULL);
	payloadsize = gAcbScanPointer - gAcbBuffer;

	*checksum = gAcbChecksum;
	*len = payloadsize + ggpo_state_header_size;
	*buffer = (unsigned char*)malloc(*len);

	int* data = (int*)*buffer;
	data[0] = 'GGPO';
	data[1] = ggpo_state_header_size;
	data[2] = nBurnVer;
	data[3] = 0;
	data[4] = 0;
	data[5] = 0;

	memcpy((*buffer) + ggpo_state_header_size, gAcbBuffer, payloadsize);
	return false;
}

bool __cdecl netcode_advance_frame_callback(int flags) {
	nFramesEmulated--;
	RunFrame(0, 0, 0);
	return true;
}

NetCode::NetCode()
{
	_eventGameStarted = CreateEvent(NULL, FALSE, FALSE, NULL);
}

NetCode::~NetCode()
{
}

bool NetCode::init()
{
	// 初始化log
	loguru::add_file("everything.log", loguru::Append, loguru::Verbosity_INFO);

	_predictFrame.frameId = -1;
	_predictFrame.data.resize(9);
	memset(_predictFrame.data.data(), 0, 9);

	createConsole();
	return connectServer();
}

void NetCode::setPlayEvent(IPlayEvent* event) {
	_playEvent = event;
}

void NetCode::setGameCallback(IGameCallback* gameCallback) {
	_gameCallback = gameCallback;
}

void NetCode::sendGameReady() {
	pb::C2S_Ready body;
	body.set_roomid(_roomId);
	body.set_playerid(_playId);

	MessageHead head;
	head.id = pb::ID::ID_Ready;
	head.body_len = body.ByteSize();

	CBufferPtr buffer;
	buffer.Cat((BYTE*)&head, sizeof(head));

	CBufferPtr body_buffer(body.ByteSize());
	body.SerializeToArray(body_buffer.Ptr(), body.ByteSize());

	buffer.Cat(body_buffer, body_buffer.Size());

	_client.send(buffer.Ptr(), buffer.Size());
}

void NetCode::waitGameStarted() {
	::WaitForSingleObject(_eventGameStarted, INFINITE);
}


void NetCode::increaseFrame() {
	saveCurrentFrameState();
	_frameId++;

	if (_is_rollback == false) {
		addRemoteInput();
	}
}

bool NetCode::getNetInput(void* values, int size, int players, bool syncOnly) {
	if (syncOnly == false) {
		if (addLocalInput((char*)values, size, players)) {
			fetchFrame(_frameId, values);
			return true;
		}
	} else {
		fetchFrame(_frameId, values);
		return true;
	}


	return false;
}


void NetCode::printLog(const std::wstring& log) {
	std::wstring logW = fmt::format(L"NetCode  {}\r\n", log);
	std::wstring* p = new std::wstring(logW);
	PostThreadMessage(_threadId, WM_PRINT_LOG, (WPARAM)p, 0);
}

bool NetCode::connectServer()
{
	auto ret = _client.createsocket(26668, "121.5.160.222");
	if (ret < 0) {
		return false;
	}

	unpack_setting_t unpack_setting;
	memset(&unpack_setting, 0, sizeof(unpack_setting_t));
	unpack_setting.mode = UNPACK_BY_LENGTH_FIELD;
	unpack_setting.package_max_length = DEFAULT_PACKAGE_MAX_LENGTH;
	unpack_setting.body_offset = sizeof(MessageHead);
	unpack_setting.length_field_offset = 0;
	unpack_setting.length_field_bytes = 4;
	unpack_setting.length_field_coding = ENCODE_BY_LITTEL_ENDIAN;
	_client.setUnpack(&unpack_setting);

	_client.onConnection = [this](const hv::SocketChannelPtr& channel) {
		if (channel->isConnected()) {
			PostThreadMessage(_threadId, WM_CONNECTED_SERVER, 0, 0);
			//PostThreadMessage(_threadId, WM_CREATE_OR_JOIN_ROOM, 0, 0);
			PostThreadMessage(_threadId, WM_AUTO_MATCH, 0, 0);
		}
	};

	_client.onMessage = [this](const hv::SocketChannelPtr& channel, hv::Buffer* buf) {
		MessageHead* head = (MessageHead*)buf->data();

		const int head_len = sizeof(MessageHead);
		const int data_len = head_len + head->body_len;

		if (buf->size() < data_len)
		{
			return;
		}

		switch (head->id) {
		case pb::ID::ID_CreateRoom:
		{
			pb::S2C_CreateRoom response;
			response.ParseFromArray(head->body, head->body_len);
			_roomId = response.roomid();
			_playId = response.playerid();
			PostThreadMessage(_threadId, WM_CREATED_ROOM, 0, 0);
			break;
		}

		case pb::ID_JoinRoom:
		{
			pb::S2C_JoinRoom response;
			response.ParseFromArray(head->body, head->body_len);
			_roomId = response.roomid();
			_playId = response.playerid();
			PostThreadMessage(_threadId, WM_JOINED_ROOM, 0, 0);
			break;
		}

		case pb::ID_S2C_WaitGameStart:
		{
			PostThreadMessage(_threadId, WM_WAIT_GAME_START, 0, 0);

			// 通知游戏准备开始
			if (_playEvent)
			{
				_playEvent->onStartGame();
			}

			PostThreadMessage(_threadId, WM_GAME_STARTED, 0, 0);

			break;
		}

		case pb::ID_Start:
		{
			//if (_eventGameStarted)
			//{
			//	::SetEvent(_eventGameStarted);
			//}
			//PostThreadMessage(_threadId, WM_GAME_STARTED, 0, 0);
			break;
		}

		case pb::ID_InputFrame: 
		{
			pb::InputFrame response;
			response.ParseFromArray(head->body, head->body_len);
			if (_playEvent)
			{
				InputData input;
				input.frameId = response.frameid();
				input.data.resize(response.input().size());
				input.data.assign(response.input().begin(), response.input().end());
				input.uuid = response.uuid();
				_playEvent->onReceiveRemoteFrame(input);
			}

			break;
		}






		}
	};

	_client.start();

	return true;
}

void NetCode::createConsole()
{
	_eventThreadStarted = CreateEvent(0, FALSE, FALSE, NULL);

	HANDLE h = CreateThread(
		NULL, 0, (LPTHREAD_START_ROUTINE)consoleThread, this, 0, &_threadId);
	if (h)
	{
		CloseHandle(h);
	}

	WaitForSingleObject(_eventThreadStarted, INFINITE);
	PostThreadMessage(_threadId, WM_CREATE_CONSOLE, 0, 0);
}


DWORD WINAPI NetCode::consoleThread(void* pParam) {
	NetCode* pThis = (NetCode*)pParam;

	MSG msg;
	PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	SetEvent(pThis->_eventThreadStarted);

	while (GetMessage(&msg, 0, 0, 0)) {
		switch (msg.message) {
		case WM_AUTO_MATCH:
		{
			printf("自动匹配比赛......\r\n");
			pThis->autoMatch();
			break;
		}
		case WM_CREATE_CONSOLE:
		{
			AllocConsole();
			freopen("CONIN$", "r", stdin);
			freopen("CONOUT$", "w", stdout);
			freopen("CONOUT$", "w", stderr);
			break;
		}

		case WM_CONNECTED_SERVER:
		{
			printf("已连接上服务器......\r\n");
			break;
		}

		case WM_CREATE_OR_JOIN_ROOM:
		{
			printf("创建或者加入房间(0-创建 1-加入):");
			int roomCmd = 0;
			scanf("%d", &roomCmd);
			if (roomCmd == 0) {
				pThis->createRoom();
			} else if (roomCmd == 1) {
				printf("请输入要加入的房间id:");
				int joinRoomId = 0;
				scanf("%d", &joinRoomId);
				pThis->joinRoom(joinRoomId);
			} else {
				printf("房间指令无效,请重新输入\r\n");
				::PostThreadMessage(pThis->_threadId, WM_CREATE_OR_JOIN_ROOM, 0, 0);
			}
			break;
		}

		case WM_CREATED_ROOM:
		{
			printf("房间已创建，id:%d，等待加入......\r\n", pThis->_roomId);
			break;
		}

		case WM_JOINED_ROOM:
		{
			printf("已加入房间, id:%d\r\n", pThis->_roomId);
			break;
		}

		case WM_WAIT_GAME_START:
		{
			printf("房间已准备就绪，服务端等待游戏开始....\r\n");
			break;
		}

		case WM_GAME_STARTED:
		{
			printf("游戏已经开始 玩家id:%d\r\n", pThis->_playId);
			break;
		}

		case WM_PRINT_LOG:
		{
			std::wstring* p = (std::wstring*)msg.wParam;
			std::wstring log = *p;
			std::string loga = w2a(log);
			printf(loga.c_str());

			LOG_F(INFO, "%s", loga.c_str());


			delete p;
		}

		}
	}

	FreeConsole();

	return 0;
}

void NetCode::createRoom() {
	pb::C2S_CreateRoom body;

	MessageHead head;
	head.id = pb::ID::ID_CreateRoom;
	head.body_len = body.ByteSize();

	CBufferPtr buffer;
	buffer.Cat((BYTE*)&head, sizeof(head));

	CBufferPtr body_buffer(body.ByteSize());
	body.SerializeToArray(body_buffer.Ptr(), body.ByteSize());

	buffer.Cat(body_buffer, body_buffer.Size());

	_client.send(buffer.Ptr(), buffer.Size());
}

void NetCode::joinRoom(::google::protobuf::uint32 roomId) {
	pb::C2S_JoinRoom body;
	body.set_roomid(roomId);

	MessageHead head;
	head.id = pb::ID::ID_JoinRoom;
	head.body_len = body.ByteSize();

	CBufferPtr buffer;
	buffer.Cat((BYTE*)&head, sizeof(head));

	CBufferPtr body_buffer(body.ByteSize());
	body.SerializeToArray(body_buffer.Ptr(), body.ByteSize());

	buffer.Cat(body_buffer, body_buffer.Size());

	_client.send(buffer.Ptr(), buffer.Size());
}

void NetCode::autoMatch() {
	MessageHead head;
	head.id = pb::ID::ID_AutoMatch;
	head.body_len = 0;

	CBufferPtr buffer;
	buffer.Cat((BYTE*)&head, sizeof(head));

	_client.send(buffer.Ptr(), buffer.Size());
}

void NetCode::sendLocalInput(const InputData& input) {
	pb::InputFrame body;
	body.set_frameid(input.frameId);
	body.set_playerid(_playId);
	body.set_roomid(_roomId);
	body.set_uuid(input.uuid);

	std::string data;
	data.resize(input.data.size());
	data.assign(input.data.begin(), input.data.end());
	body.set_input(data);

	MessageHead head;
	head.id = pb::ID::ID_InputFrame;
	head.body_len = body.ByteSize();

	CBufferPtr buffer;
	buffer.Cat((BYTE*)&head, sizeof(head));

	CBufferPtr body_buffer(body.ByteSize());
	body.SerializeToArray(body_buffer.Ptr(), body.ByteSize());

	buffer.Cat(body_buffer, body_buffer.Size());

	_client.send(buffer.Ptr(), buffer.Size());
}

void NetCode::fetchFrame(int id, void* values) {
	// 获取本地帧
	InputData local = _localInputMap[id];

	// 获取远端帧
	bool remoteFrameAvailable = (_remoteInputMap.find(id) != _remoteInputMap.end());

	InputData remote;
	remote.frameId = id;


	std::wstring logFetchRemoteFrameType;
	// 从本地获取
	// 从远端获取，若远端存在则直接获取，否则预测(如果远端存在，则用最后一个作为预测，否则使用上一次的预测值)

	// 远端存在则获取
	if (remoteFrameAvailable) {
		remote = _remoteInputMap[_frameId];

		logFetchRemoteFrameType = L"已存在直接获取";
	}
	// 不存在则预测
	else {
		_predictFrame.frameId = id;

		// 如果远端缓存中未找到，如果远端缓存不为空，则用最后一个作为预测
		if (_remoteInputMap.size() > 0) {

			auto itEnd = _remoteInputMap.end();
			--itEnd;

			InputData lastRemoteFrame = itEnd->second;

			_predictFrame.data.resize(lastRemoteFrame.data.size());
			_predictFrame.data.assign(lastRemoteFrame.data.begin(), lastRemoteFrame.data.end());

			logFetchRemoteFrameType = L"使用远端上一帧的值";

		} else {
			logFetchRemoteFrameType = L"使用上一次预测值";
		}

		remote = _predictFrame;
	}

	CBufferPtr buffer;
	buffer.Cat((BYTE*)local.data.data(), local.data.size());
	buffer.Cat((BYTE*)remote.data.data(), remote.data.size());

	// 打印预测之后的帧
	printLog(L"");
	printLog(fmt::format(L"[fetch]-----获取帧id:{} 远端帧获取方式为:{} 预测的id:{}, 远端帧缓存数量为:{}", 
		id, logFetchRemoteFrameType, _predictFrame.frameId, _remoteInputMap.size()));
	printLog(fmt::format(L"[fetch]-----帧数据 本地帧:{} uuid:{}", a2w(local.uuid), formatFrameData(local)));
	printLog(fmt::format(L"[fetch]-----帧数据 远端帧:{} uuid:{}", a2w(local.uuid), formatFrameData(remote)));
	printLog(L"");

	unsigned char* buf = buffer.Ptr();
	memcpy(values, buffer.Ptr(), buffer.Size());
}

bool NetCode::addLocalInput(char* values, int size, int players) {
	if (_is_rollback) {
		printLog(fmt::format(L"[local]-----正在回滚，本地添加第{}帧失败", _frameId));
		return false;
	}


	if (_frameId == -1) {
		saveCurrentFrameState();
		_frameId += 1;
	}

	auto it = _localInputMap.find(_frameId);
	if (it == _localInputMap.end()) {
		InputData local;
		local.frameId = _frameId;
		local.data.resize(size);
		memcpy(local.data.data(), values, size);
		_localInputMap[_frameId] = local;
		local.uuid = generate();

		sendLocalInput(local);
		printLog(fmt::format(L"[local]-----添加本地帧id:{}, uuid:{}, 数据:{}, 并发送给远端", _frameId, a2w(local.uuid), formatFrameData(local)));

	} else {
		printLog(fmt::format(L"[local]-----添加本地帧id:{}, uuid:{}, 数据:{}, 已缓存无需重复添加 ", _frameId, a2w(_localInputMap[_frameId].uuid), formatFrameData(_localInputMap[_frameId])));
	}

	return true;
}


void NetCode::addRemoteInput() {
	bool isPredOk = true;
	while (_remoteInputNetCacheQueue.empty() == false) {
		auto remoteFrame = _remoteInputNetCacheQueue.front();
		printLog(fmt::format(L"[remote]-----远端帧id:{} uuid:{} 到达，准备判断是否预测成功",  remoteFrame.frameId, a2w(remoteFrame.uuid)));

		if (remoteFrame.frameId <= _predictFrame.frameId) {
			printLog(
				fmt::format(L"[remote]-----本地已经预测过且之前已预测的远端帧id:{}到达, remoteFrame.frameId:{} <= _predictFrame.frameId:{}", 
					remoteFrame.frameId, remoteFrame.frameId, _predictFrame.frameId));

			_remoteInputMap[remoteFrame.frameId] = remoteFrame;

			// 判断是否预测失败
			if (isPredOk == true)
			{
				if (cmpInputData(remoteFrame, _predictFrame) != 0) {
					isPredOk = false;
					_firstPredictFrameId = remoteFrame.frameId;

					printLog(L"=========================================================================");
					printLog(fmt::format(L"[remote]-----远端帧id:{} uuid:{} 预测失败, 第{}帧预测错误, 需要回滚", remoteFrame.frameId, a2w(remoteFrame.uuid), _firstPredictFrameId));
					printLog(fmt::format(L"[remote]-----远端:{}", formatFrameData(remoteFrame)));
					printLog(fmt::format(L"[remote]-----预测:{}", formatFrameData(_predictFrame)));
					printLog(L"=========================================================================");
				} else {
					printLog(L"=========================================================================");
					printLog(fmt::format(L"[remote]-----远端帧id:{} uuid:{} 预测成功", remoteFrame.frameId, a2w(remoteFrame.uuid)));
					printLog(fmt::format(L"[remote]-----远端:{}", formatFrameData(remoteFrame)));
					printLog(fmt::format(L"[remote]-----预测:{}", formatFrameData(_predictFrame)));
					printLog(L"=========================================================================");
				}
			}
			_remoteInputNetCacheQueue.pop();

			break;
		} else {
			printLog(
				fmt::format(L"[remote]-----新的远端帧 id:{} uuid:{} 到达, 超出范围，忽略 remoteFrame.frameId:{} > _predictFrame.frameId:{}",
					remoteFrame.frameId, a2w(remoteFrame.uuid), remoteFrame.frameId, _predictFrame.frameId));
			break;
		}
	}
}

void NetCode::checkRollback() {
	if (_firstPredictFrameId != -1) {

		// 查找错误帧id的上一帧快照
		printLog(fmt::format(L"[rollback]-----查询预测错误帧id:{}上一帧的快照(快照对应帧id:{})", _firstPredictFrameId, _firstPredictFrameId - 1));
		auto it = _savedFrame.find(_firstPredictFrameId - 1);
		if (it != _savedFrame.end()) {
			printLog(fmt::format(L"[rollback]-----帧id:{}对应快照存在", _firstPredictFrameId));
			_is_rollback = true;

			int f = _firstPredictFrameId;
			printLog(fmt::format(L"[rollback]-----帧id:{}, 需要回滚，正在回滚到第{}帧", 
				_firstPredictFrameId, _firstPredictFrameId -1 ));

			SavedFrame state = _savedFrame[_firstPredictFrameId - 1];
			_gameCallback->load_game_state(state.buf, state.bufCounts);

			_frameId = _firstPredictFrameId;
			_firstPredictFrameId = -1;

			_predictFrame.frameId = -1;

			//_gameCallback->advance_frame(0);

			_is_rollback = false;

			printLog(fmt::format(L"[rollback]-----回滚到第{}帧结束", f));
		} else {
			printLog(fmt::format(L"[rollback]-----帧id:{}对应快照不存在", _firstPredictFrameId));
		}
	}
}

void NetCode::saveCurrentFrameState() {
	if (_gameCallback)
	{
		SavedFrame frameState = {0};
		frameState.frameId = _frameId;
		_gameCallback->save_game_state(&frameState.buf, &frameState.bufCounts, &frameState.checksum, frameState.frameId);
		_savedFrame[_frameId] = frameState;

		printLog(fmt::format(L"[save]-----保存第{}帧快照", _frameId));
	}
}

int NetCode::cmpInputData(const InputData& input1, const InputData& input2) {
	return memcmp(input1.data.data(), input2.data.data(), input1.data.size());
}

std::wstring NetCode::formatFrameData(const InputData& inputFrame) {
	std::string fmtLog;
	fmtLog = "[";
	for (auto d : inputFrame.data) {
		char r[25];
		itoa(d, r, 10);

		fmtLog += r;
		fmtLog += " ";
	}
	fmtLog += "]";

	return a2w(fmtLog);
}

std::string NetCode::generate() {
#define GUID_LEN 64
	char buf[GUID_LEN] = { 0 };
	GUID guid;

	if (CoCreateGuid(&guid)) {
		return std::move(std::string(""));
	}

	sprintf(buf,
		"%08X-%04X-%04x-%02X%02X-%02X%02X%02X%02X%02X%02X",
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2],
		guid.Data4[3], guid.Data4[4], guid.Data4[5],
		guid.Data4[6], guid.Data4[7]);

	return std::move(std::string(buf));
}

void NetCode::receiveRemoteFrame(const InputData& remoteFrame) {
	_remoteInputNetCacheQueue.push(remoteFrame);

	// 保存远端帧
	//auto it = _remoteInputMap.find(remoteFrame.frameId);
	//if (it == _remoteInputMap.end()) {
	//	printLog(fmt::format(L"[remote]收到远端帧 id:{}", remoteFrame.frameId));
	//	_remoteInputMap[remoteFrame.frameId] = remoteFrame;
	//}


	//if (_predictFrame.frameId >= 0) {
	//	if (cmpInputData(remoteFrame, _predictFrame) != 0) {
	//		printLog(fmt::format(L"[predict]远端帧id:{}预测失败，打回滚标记", remoteFrame.frameId));
	//		_firstPredictFrameId = remoteFrame.frameId;
	//	} else {
	//		printLog(fmt::format(L"[predict]远端帧id:{}预测成功", remoteFrame.frameId));
	//	}
	//}
}