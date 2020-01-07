#include "P2P.h"

NetRouter::NetRouter(ULONG _parent) : isRunning(true), listener(make_tcpSocket()), parent(make_tcpSocket()), parentid(_parent), orderer(5, move(_parent)), rep_addr(INADDR_NONE), newdata(false)
{
	listener.SetAddress(INADDR_ANY, port_router);
	listener.Bind();
	listener.Listen(5);
	AddListenEvent(listener.getHandle(), WSACreateEvent(), INADDR_NONE);
	ConnectToParent(_parent);
}

void NetRouter::Start()
{
	routing = move(thread([](NetRouter* router) { router->Stacking(); }, this));
	tasking = move(thread([](NetRouter* router) { router->Tasking(); }, this));
	subtasking = move(thread([](NetRouter* router) { router->SubTasking(); }, this));
}

void NetRouter::Stacking()
{
	WSANETWORKEVENTS networkEvents;
	DWORD idx;
	CBuffer cBuffer(1024);
	register int frame = 0;
	while (isRunning) {
		if (++frame == 3) {
			CalculateOptimalNode();
			OutputQueue(PacketPair{ parentid, PacketFactory::NetPacketChildCnt(cntChild) });
			if (parentid == INADDR_NONE)
				OutputQueue(PacketPair{ parentid, PacketFactory::NetPacketIP(orderer.getValue()) });
			else
				OutputQueue(PacketPair{ parentid, PacketFactory::NetPacketIP(parentid) });
			frame = 0;
		}
		

		idx = WSAWaitForMultipleEvents(m_arrEvt.size(), m_arrEvt.getArray(), FALSE, 1000, FALSE);
		if (idx == WSA_WAIT_FAILED || idx == WSA_WAIT_TIMEOUT)
			continue;
		idx = idx - WSA_WAIT_EVENT_0;
		if (WSAEnumNetworkEvents(m_arrIn.at(idx).socket, m_arrEvt.at(idx), &networkEvents) == SOCKET_ERROR)
			continue;

		auto& st = m_arrIn.at(idx);

		if (networkEvents.lNetworkEvents & FD_ACCEPT) {
			if (networkEvents.iErrorCode[FD_ACCEPT_BIT] == 0) {
				ULONG addr;
				SOCKET s(TcpListener::Accept(st.socket, &addr));
				if (m_arrEvt.size() >= NetMaxChild)
				{
					try {
						auto buf = PacketFactory::NetPacketIP(orderer.getValue(), NetPacketFlag::REFUSE);
						PerfectSend(s, buf);
						
					} catch (...) {}
					shutdown(s, SD_BOTH);
					closesocket(s);
				}
				else
				{
					try {
						auto buf = PacketFactory::NetPacketIP(parentid, NetPacketFlag::CONN);
						PerfectSend(s, buf);
						AddEvent(s, WSACreateEvent(), addr);
						++cntChild;
					}
					catch (...) {
						closesocket(s);
					}
				}
			}
		}

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
			if (m_arrIn.at(idx).socket == parent) {
				parent = make_tcpSocket();
				parentid = INADDR_NONE;
				printf("end conn");
				if (rep_addr != INADDR_NONE)
					async(launch::async, [](NetRouter* r, ULONG addr) { r->ConnectToParent(addr); }, this, rep_addr);
			}
				--cntChild;
			RemoveEvent(idx);
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
	while (!m_outputBuffer.empty()) {
		m_outputBuffer.pop();
	}
}

void NetRouter::StopListening()
{
	listener.Close();
}

void NetRouter::CalculateOptimalNode()
{
	for (int i = 0; i < m_arrIn.size(); ++i) {
		orderer.Update(m_arrIn.at(i).cntChild, m_arrIn.at(i).id);
	}
}

void NetRouter::Tasking()
{
	while (isRunning) {
		PacketPair pair;
		{
			unique_lock<mutex> lk(m_packetLock);
			lk.lock();
			if (m_packetBuffer.empty())
			{
				lk.unlock();
				Sleep(150);
				continue;
			}
			pair = move(m_packetBuffer.front());
			m_packetBuffer.pop();
			lk.unlock();
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
		case NetPacketProtocol::CHILDCOUNT:
		{
			try {
				lock_guard<mutex> lk(m_modifyLock);
				for (int i = 0; i < m_arrIn.size(); ++i)
				{
					if (pair.id == m_arrIn.at(i).id) {
						m_arrIn.at(i).cntChild = *reinterpret_cast<BYTE*>(pair.buffer.getBuffer() + offset);
						break;
					}
				}
			}
			catch (...) {}
			break;
		}
		case NetPacketProtocol::QUERY:
		{
			printf("QUERY: %s\n", pair.buffer.getBuffer() + offset);
		}
		default:
		{
			OutputQueue(move(pair));
			break;
		}
		}
	}
}

void NetRouter::SubTasking()
{
	PacketPair pair;
	while (isRunning) { //최적화 필요
		{
			unique_lock<mutex> lk(m_outputLock);
			if (!m_outputBuffer.empty())
				lk.lock();
			else
				cv.wait(lk, [&] {return !m_outputBuffer.empty() || !isRunning; });
			if (!isRunning) {
				lk.unlock();
				return;
			}
			lk.unlock();

			while (true) {
				lk.lock();
				if (!m_outputBuffer.empty()) {
					pair = move(m_outputBuffer.front());
					m_outputBuffer.pop();
					lk.unlock();
					BroadCastToNodes(pair.buffer, pair.id);
				}
				else {
					lk.unlock();
					break;
				}
			}
		}

		
	}
}

void NetRouter::ConnectToParent(ULONG addr)
{
	if (addr == INADDR_NONE)
		return;
	sockaddr_in _addr;
	_addr.sin_addr.S_un.S_addr = addr;
	_addr.sin_port = htons(port_router);
	_addr.sin_family = AF_INET;

	CBuffer buffer;
	do {
		if (connect(parent, reinterpret_cast<sockaddr*>(&_addr), sizeof(_addr)) == -1)
			return;
		PerfectRecv(parent, buffer);
		memcpy(&_addr.sin_addr.S_un.S_addr, buffer.getBuffer() + PacketFactory::szHeader, sizeof(ULONG));
		
	} while ((*reinterpret_cast<NetPacketHeader*>(buffer.getBuffer())).flag != NetPacketFlag::CONN);

	parentid = _addr.sin_addr.S_un.S_addr;
	AddEvent(parent, WSACreateEvent(), parentid);
	OutputQueue(PacketPair{ parentid, PacketFactory::NetPacketIP(parentid) });
}

void NetRouter::BroadCastToNodes(CBuffer& packet, const ULONG& filter)
{
	lock_guard<mutex> lk(m_modifyLock);
	for (int i = 1; i < m_arrIn.size(); ++i)
	{
		auto p = m_arrIn.at(i);
		if (p.id == filter)
			continue;
		try {
			PerfectSend(p.socket, packet);
		}
		catch (SocketException e) {
		} //로그
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

void NetRouter::AddEvent(SOCKET s, WSAEVENT e, ULONG addr) //Stacking만 호출.
{
	WSAEventSelect(s, e, FD_READ | FD_CLOSE);
	auto si = SOCKETIN{ -1,0,s,addr };
	{
		lock_guard<mutex> lk(m_modifyLock);
		m_arrIn.push((si));
		m_arrEvt.push(e);
	}
}

void NetRouter::AddListenEvent(SOCKET s, WSAEVENT e, ULONG addr) //Stacking만 호출.
{
	WSAEventSelect(s, e, FD_ACCEPT | FD_READ | FD_CLOSE);
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

	routing.join();
	tasking.join();
	subtasking.join();

	closesocket(parent);

	ClearStacking();
}
