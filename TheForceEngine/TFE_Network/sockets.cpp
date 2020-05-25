#include "sockets.h"

#if 0
TODO: Finish
Based on https://gafferongames.com/post/sending_and_receiving_packets/
#endif

#ifdef _WIN32
	#include <winsock2.h>
	#pragma comment(lib, "wsock32.lib")
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <fcntl.h>
#endif

// Static setup.
bool TFE_Socket::initSocketLayer()
{
#ifdef _WIN32
	WSADATA WsaData;
	return WSAStartup(MAKEWORD(2, 2), &WsaData) == NO_ERROR;
#else
	return true;
#endif
}

void TFE_Socket::shutdownSocketLayer()
{
#ifdef _WIN32
	WSACleanup();
#endif
}

TFE_Socket::TFE_Socket()
{
}

TFE_Socket::~TFE_Socket()
{
}

bool TFE_Socket::open(u16 port)
{
	m_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_handle <= 0)
	{
		return false;
	}

	return true;
}

void TFE_Socket::close()
{
}

bool TFE_Socket::isOpen() const
{
}

bool TFE_Socket::send(const IpAddress& destination, const void* data, s32 size)
{
}

s32  TFE_Socket::receive(IpAddress& sender, void* data, s32 size)
{
}
