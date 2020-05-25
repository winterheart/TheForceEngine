#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Sockets
// Based on using UDP only.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

class IpAddress
{
public:
	IpAddress();
	IpAddress(u8 a, u8 b, u8 c, u8 d, u16 port);
	IpAddress(u32 address, u16 port);

	u32 getAddress() const;
	u8  getA() const;
	u8  getB() const;
	u8  getC() const;
	u8  getD() const;

	u16 getPort() const;

private:
	u32 m_address;
	u16 m_port;
};

class TFE_Socket
{
public:
	static bool initSocketLayer();
	static void shutdownSocketLayer();

	TFE_Socket();
	~TFE_Socket();

	bool open(u16 port);
	void close();

	bool isOpen() const;
	bool send(const IpAddress& destination, const void* data, s32 size);
	s32  receive(IpAddress& sender, void* data, s32 size);

private:
	s32 m_handle;
};