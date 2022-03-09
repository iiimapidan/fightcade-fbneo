#include "NetCode.h"

NetCode::NetCode()
{

}

NetCode::~NetCode()
{
}

bool NetCode::init()
{
	return connectServer();
}

bool NetCode::connectServer()
{
	auto ret = _client.createsocket(26668, "121.5.160.222");
	if (ret < 0)
	{
		return false;
	}

	_client.onConnection = [](const SocketChannelPtr& channel) {

	};

	_client.onMessage = [](const SocketChannelPtr& channel, Buffer* buf) {

	};

	_client.start();

	return true;
}
