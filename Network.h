#pragma once
#include <WinSock2.h>
#include <memory>
#pragma comment(lib, "ws2_32.lib")
#include "Buffer.h"
using namespace std;

class SocketRAII {
private:
	SOCKET m_socket;
public:
	SocketRAII(SOCKET socket);
	SocketRAII(SocketRAII&& socket) noexcept;
	~SocketRAII();
	void reset(SOCKET socket);
	void release();
	SocketRAII& operator=(SocketRAII&& other) noexcept;
	SOCKET getHandle() const;
};

class SocketException
{
public:
	enum E {
		DISCONN,
		REFUSE,
		BUFFER
	};
private:
	int ecode;
public:
	SocketException(int code) : ecode(code) {};
	int GetErrorCode() const { return ecode; }
};

class TcpCommunicator
{
private:
	SOCKET m_socket;
public:
	TcpCommunicator(SOCKET socket);
	~TcpCommunicator();

	unique_ptr<CBuffer> Receive();
	auto Send(const CBuffer& buffer);
	void reset(SOCKET socket);
	auto getHandle()  const {
		return m_socket;
	}


	void Shutdown(int how);
	void Close();
};

class TcpListener
{
private:
	SOCKET m_socket;
	sockaddr_in m_addr;
public:
	TcpListener(SOCKET);
	~TcpListener();
	void SetAddress(const ULONG& address, int port);
	void Listen(int backlog);
	void Bind();
	SOCKET Accept();
	SOCKET Accept(ULONG* addr);
	void Shutdown(int how);
	void Close();
	auto getHandle()  const {
		return m_socket;
	}

	static SOCKET Accept(SOCKET s, ULONG* addr)
	{
		sockaddr_in _addr;
		int sz = sizeof(sockaddr_in);
		auto as = accept(s, (sockaddr*)&_addr, &sz);
		*addr = _addr.sin_addr.S_un.S_addr;
		return as;
	}
};

SOCKET make_tcpSocket();
void Bind(SOCKET s, const ULONG& address, int port);
void PerfectSend(SOCKET s, CBuffer& buffer);
void PerfectRecv(SOCKET s, CBuffer& buffer);