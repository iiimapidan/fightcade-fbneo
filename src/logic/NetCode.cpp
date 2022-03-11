#include "NetCode.h"

#include <stdio.h>
#include <sstream>
#include <string>

#include "bufferptr.h"
#include "InstanceWindow.h"
#include "utils/fmt/format.h"

#pragma pack(1)
typedef struct _MsgHead {
	uint32_t body_len;
	uint32_t id;
	uint64_t timestamp;
	BYTE* body[0];
}MessageHead;

#pragma pack()



NetCode::NetCode()
{
	_eventGameStarted = CreateEvent(NULL, FALSE, FALSE, NULL);
}

NetCode::~NetCode()
{
}

bool NetCode::init()
{
	_frameId = 0;

	_predictFrame.frameId = -1;
	_predictFrame.data.resize(9);
	memset(_predictFrame.data.data(), 0, 9);

	createConsole();
	return connectServer();
}

void NetCode::setPlayEvent(IPlayEvent* event) {
	_playEvent = event;
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
	_frameId++;
}

bool NetCode::getNetInput(void* values, int size, int players) {
	InputData local;
	local.frameId = _frameId;
	local.data.resize(size);
	memcpy(local.data.data(), values, size);

	// 将当前帧添加到本地
	_localInputMap[_frameId] = local;

	// 发送本地帧到远端
	sendLocalInput(local);

	// 获取远端帧
	bool remoteFrameAvailable = (_remoteInputMap.find(_frameId) != _remoteInputMap.end());

	InputData remote;
	remote.frameId = _frameId;

	// 远端帧存在则获取
	if (remoteFrameAvailable) {
		remote = _remoteInputMap[_frameId];
	}
	// 不存在则预测
	else {
		_predictFrame.frameId = _frameId;
		remote.data = _predictFrame.data;
	}

	return false;
}

void NetCode::receiveRemoteFrame(const InputData& remoteFrame) {
	_remoteInputMap[remoteFrame.frameId] = remoteFrame;

	printLog(fmt::format(L"收到远端帧 id:{}", remoteFrame.frameId));
}

void NetCode::printLog(const std::wstring& log) {
	OutputDebugString(fmt::format(L"NetCode  {}\r\n", log).c_str());
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
			PostThreadMessage(_threadId, WM_CREATE_OR_JOIN_ROOM, 0, 0);
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

				_playEvent->onReceiveRemoteFrame(input);
			}
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

void NetCode::sendLocalInput(const InputData& input) {
	pb::InputFrame body;
	body.set_frameid(input.frameId);
	body.set_playerid(_playId);
	body.set_roomid(_roomId);
	//body.set_input(input.data);

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
