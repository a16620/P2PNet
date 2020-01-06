#include "P2P.h"

NetRouter::NetRouter() : isRunning(true), listener(make_tcpSocket()), parent(make_tcpSocket()), parentid(INADDR_NONE)
{
	listener.SetAddress(INADDR_ANY, port_router);
	listener.Bind();
	listener.Listen(5);
}

void NetRouter::Start()
{
	listening = move(thread([](NetRouter* router) { router->Listening(); }, this));
	routing = move(thread([](NetRouter* router) { router->Stacking(); }, this));
	tasking = move(thread([](NetRouter* router) { router->Tasking(); }, this));
}

void NetRouter::Stacking()
{
	WSANETWORKEVENTS networkEvents;
	DWORD idx;
	CBuffer cBuffer(1024);
	while (isRunning) {
		idx = WSAWaitForMultipleEvents(m_arrEvt.size(), m_arrEvt.getArray(), FALSE, 1000, FALSE);
		if (idx == WSA_WAIT_FAILED || idx == WSA_WAIT_TIMEOUT)
			continue;
		idx = idx - WSA_WAIT_EVENT_0;
		if (WSAEnumNetworkEvents(m_arrIn.at(idx).socket, m_arrEvt.at(idx), &networkEvents) == SOCKET_ERROR)
			continue;

		auto& st = m_arrIn.at(idx);
		if (networkEvents.lNetworkEvents & FD_READ) {
			int readbytes = recv(st.socket, cBuffer.getBuffer(), 1024, 0);
			if (readbytes == 0)
				continue;

			st.buffer.push(cBuffer, readbytes);
			if (st.needSize == -1) {
				if (st.buffer.Used() >= 4) {
					st.buffer.poll(cBuffer, 4);
					st.needSize = ntohl(*reinterpret_cast<int*>(cBuffer.getBuffer()));
				}
			}
			
			if (st.needSize != -1 && st.buffer.Used() >= st.needSize) {
				st.buffer.poll(cBuffer, st.needSize);
				lock_guard<mutex> lk(m_packetLock);
				m_packetBuffer.push(PacketPair{ st.id, move(cBuffer.slice(st.needSize)) });
				st.needSize = -1;
			}
			
		}

		if (networkEvents.lNetworkEvents & FD_CLOSE) {
			if (networkEvents.iErrorCode[FD_CLOSE_BIT]) {
				if (m_arrIn.at(idx).socket == parent) {
					parent = make_tcpSocket();
					parentid = INADDR_NONE;
					if (rep_addr != INADDR_NONE)
						async(launch::async, [](NetRouter* r, ULONG addr) { r->ConnectToParent(addr); }, this, rep_addr);
				}
				RemoveEvent(idx);
			}
		}
	}
}

void NetRouter::ClearStacking()
{
	while (m_arrIn.size()) {
		closesocket(m_arrIn.at(0).socket);
		m_arrIn.pop(0);
		WSACloseEvent(m_arrEvt.at(0));
		m_arrEvt.pop(0);
	}
	while (!m_packetBuffer.empty()) {
		m_packetBuffer.pop();
	}
}

void NetRouter::Listening()
{
	while (isRunning)
	{
		ULONG addr;
		SOCKET s(listener.Accept(&addr));
		if (m_arrEvt.size() >= NetMaxChild)
		{
			shutdown(s, SD_BOTH);
			closesocket(s);
		}
		else
		{
			AddEvent(s, WSACreateEvent(), addr);
			OutputQueue(PacketPair{ parentid, PacketFactory::NetPacketIP(parentid) });
		}
	}
}

void NetRouter::StopListening()
{
	listener.Close();
}

void NetRouter::Tasking()
{
	while (isRunning) {
		PacketPair pair;
		{
			lock_guard<mutex> lk(m_packetLock);
			if (m_packetBuffer.empty())
			{
				Sleep(150);
				continue;
			}
			pair = move(m_packetBuffer.front());
			m_packetBuffer.pop();
		}
		auto bytes = pair.buffer.getBuffer();
		auto offset = PacketFactory::szHeader;
		
		switch (reinterpret_cast<NetPacketHeader*>(bytes)->protocol)
		{
		case NetPacketProtocol::REPLACE_ADDR:
		{
			if (pair.id == parentid)
			{
				memcpy(&rep_addr, pair.buffer.getBuffer() + offset, sizeof(ULONG));
			}
			break;
		}
		case NetPacketProtocol::QUERY:
		{
			printf("QUERY: %s", pair.buffer.getBuffer() + offset);
			break;
		}
		}

		OutputQueue(move(pair));
	}
}

void NetRouter::SubTasking()
{
	PacketPair pair;
	while (true) {
		{
			unique_lock<mutex> lk(m_outputLock);
			cv.wait(lk, [&] {return !m_outputBuffer.empty() || !isRunning; });
			if (!isRunning) {
				lk.unlock();
				return;
			}
			pair = move(m_outputBuffer.front());
			m_outputBuffer.pop();
		}

		BroadCastToNodes(pair.buffer, pair.id);
	}
}

void NetRouter::ConnectToParent(ULONG addr)
{
	sockaddr_in _addr;
	_addr.sin_addr.S_un.S_addr = addr;
	_addr.sin_port = htons(port_router);
	_addr.sin_family = AF_INET;

	connect(parent, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)); //설계 필요

	parentid = addr;
	OutputQueue(PacketPair{ addr, PacketFactory::NetPacketIP(parentid) });
}

void NetRouter::BroadCastToNodes(CBuffer& packet, const ULONG& filter)
{
	lock_guard<mutex> lk(m_modifyLock);
	for (int i = 0; i < m_arrIn.size(); ++i)
	{
		auto s = m_arrIn.at(i).socket;
		if (s == filter)
			continue;
		try {
			PerfectSend(s, packet);
		}
		catch (SocketException e) {}
	}
}

void NetRouter::OutputQueue(PacketPair&& packet)
{
	{
		lock_guard<mutex> lk(m_outputLock);
		m_outputBuffer.push(move(packet));
	}
	cv.notify_one();
}

void NetRouter::AddEvent(SOCKET s, WSAEVENT e, ULONG addr)
{
	WSAEventSelect(s, e, FD_READ | FD_CLOSE);
	auto si = SOCKETIN{ -1,0,s,addr };
	{
		lock_guard<mutex> lk(m_modifyLock);
		m_arrIn.push((si));
		m_arrEvt.push(e);
	}
}

void NetRouter::RemoveEvent(int idx) //Stacking만 호출.
{
	lock_guard<mutex> lk(m_modifyLock);
	shutdown(m_arrIn.at(idx).socket, SD_BOTH);
	closesocket(m_arrIn.at(idx).socket);
	m_arrIn.pop(idx);
	WSACloseEvent(m_arrEvt.at(idx));
	m_arrEvt.pop(idx);
}

void NetRouter::Stop()
{
	isRunning = false;
	cv.notify_one();
	StopListening();

	listening.join();
	routing.join();
	tasking.join();

	ClearStacking();
}
