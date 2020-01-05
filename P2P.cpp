#include "P2P.h"
#include <array>

NetRouter::NetRouter() : isRunning(true), listener(make_tcpSocket()), m_Bmutex(), m_Smutex(), m_buffer()
{
	Bind(listener.getHandle(), INADDR_ANY, port_router);
	listen(listener.getHandle(), 3);
	
	WSAEVENT evt = WSACreateEvent();
	WSAEventSelect(listener.getHandle(), evt, FD_ACCEPT | FD_CLOSE);
	AddEvent(listener.getHandle(), evt);
}

void NetRouter::Routing()
{
	WSANETWORKEVENTS networkEvents;
	while (isRunning) {
		int idx = WSAWaitForMultipleEvents(m_arrEvt.size(), m_arrEvt.getArray(), FALSE, WSA_INFINITE, FALSE);
		if (idx == WSA_WAIT_FAILED)
			continue;
		if (WSAEnumNetworkEvents(m_arrCon.at(idx).getHandle(), m_arrEvt.at(idx), &networkEvents))
			continue;
		
		if (networkEvents.lNetworkEvents & FD_ACCEPT) {

		}
	}
}

void NetRouter::EndRouting()
{
}

void NetRouter::Listening()
{
	
}

void NetRouter::EndListening()
{
	shutdown(listener.getHandle(), SD_BOTH);
	listener.reset();
}

void NetRouter::Tasking()
{
	CBuffer buffer;
	{
		lock_guard<mutex> lk(m_Bmutex);
		if (m_buffer.empty())
			return;
		buffer = move(m_buffer.front());
		m_buffer.pop();
	}
	auto bytes = buffer.getBuffer();
	if (reinterpret_cast<NetPacketHeader*>(bytes)->from) {
		buffer.release();
		return;
	}

}

void NetRouter::Refresh()
{
	
}

void NetRouter::Replace()
{
}

void NetRouter::AddEvent(SOCKET s, WSAEVENT e)
{
	m_arrEvt.push(e);
	m_arrCon.push(s);
}

bool NetRouter::Join()
{
	return false;
}

void NetRouter::End()
{
	isRunning = false;
	EndRouting();
	EndListening();
}
