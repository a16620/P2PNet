#include "Network.h"

SocketRAII::SocketRAII(SOCKET socket) : m_socket(socket)
{
}

SocketRAII::SocketRAII(SocketRAII&& socket) noexcept
{
	m_socket = socket.m_socket;
	socket.m_socket = INVALID_SOCKET;
}

SocketRAII::~SocketRAII()
{
	if (m_socket != INVALID_SOCKET)
		closesocket(m_socket);
}

void SocketRAII::reset(SOCKET socket=INVALID_SOCKET)
{
	if (m_socket != INVALID_SOCKET)
		closesocket(m_socket);
	m_socket = socket;
}

SocketRAII& SocketRAII::operator=(SocketRAII&& other) noexcept
{
	m_socket = other.m_socket;
	other.m_socket = INVALID_SOCKET;
	return *this;
}

SOCKET SocketRAII::getHandle() const
{
	return m_socket;
}

TcpCommunicator::TcpCommunicator(SocketRAII&& socket) : m_socket(move(socket))
{
}

TcpCommunicator::~TcpCommunicator()
{
	Close();
}

unique_ptr<CBuffer> TcpCommunicator::Receive()
{
	long len = 0, t = 0;
	do {
		int t_;
		t_ = recv(m_socket.getHandle(), ((char*)&len) + t, 4 - t, 0);
		if (t_ == -1)
		{
			throw SocketException(SocketException::E::DISCONN);
		}
		else
			t += t_;
	} while (t < 4);

	len = ntohl(len);
	auto buffer = make_unique<CBuffer>(len);
	t = 0;
	do {
		int t_;
		t_ = recv(m_socket.getHandle(), ((char*)buffer->getBuffer()) + t, len - t, 0);
		if (t_ == -1)
		{
			buffer.release();
			throw SocketException(SocketException::E::DISCONN);
		}
		else
			t += t_;
	} while (t < len);

	return buffer;
}

auto TcpCommunicator::Send(const CBuffer& buffer)
{
	int sz = buffer.size();
	if (sz == 0)
	{
		throw SocketException(SocketException::E::BUFFER);
	}
	long _sz = htonl(sz);
	int r = 0;
	do {
		auto res = send(m_socket.getHandle(), (char*)&_sz, 4, 0);
		if (res == -1)
			throw SocketException(SocketException::E::DISCONN);
		else
			r += res;
	} while (r < 4);

	r = 0;
	do
	{
		int r_ = send(m_socket.getHandle(), buffer.getBuffer() + r, sz - r, 0);
		if (r_ == -1)
			throw SocketException(SocketException::E::DISCONN);
		r += r_;
	} while (r < sz);

	return;
}

void TcpCommunicator::reset(SocketRAII&& socket)
{
	m_socket = move(socket);
}

void TcpCommunicator::Shutdown(int how)
{
	shutdown(m_socket.getHandle(), how);
}

void TcpCommunicator::Close()
{
	m_socket.reset();
}

TcpListener::TcpListener(SocketRAII&& socket) : m_socket(move(socket))
{
	ZeroMemory(&m_addr, sizeof(sockaddr_in));
	m_addr.sin_family = AF_INET;
}

TcpListener::~TcpListener()
{
	Close();
}

void TcpListener::SetAddress(const ULONG& address, int port)
{
	m_addr.sin_addr.s_addr = htonl(address);
	m_addr.sin_port = htons(port);
}

void TcpListener::Listen(int backlog)
{
	listen(m_socket.getHandle(), backlog);
}

void TcpListener::Bind()
{
	bind(m_socket.getHandle(), (sockaddr*)&m_addr, sizeof(sockaddr_in));
}

SocketRAII TcpListener::Accept()
{
	sockaddr_in addr;
	int sz = sizeof(sockaddr_in);
	return SocketRAII(accept(m_socket.getHandle(), (sockaddr*)&addr, &sz));
}

void TcpListener::Shutdown(int how)
{
	shutdown(m_socket.getHandle(), how);
	Close();
}

void TcpListener::Close()
{
	m_socket.reset();
}