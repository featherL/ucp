#include "ucpserver.hpp"

#include <cstddef>
#include <cstring>
#include <cstdio>
#include <memory>
#include <mutex>

#include "kcp/ikcp.h"
#include "ucpbase.hpp"

using namespace ucp;

void ServerInternel::monitor_thread_func(
	std::shared_ptr<Sock> sock, std::shared_ptr<ServerInternel> internel)
{
	while (true) {
		{
			std::lock_guard<std::mutex> lock(internel->status_mutex_);
			if (internel->status_ == kInit) {
				internel->status_ = tranfer_status_from_init(sock, internel);
			} else if (internel->status_ == kListen) {
				internel->status_ = tranfer_status_from_listen(sock, internel);
			} else if (internel->status_ == kClosed) {
				break;
			} else if (internel->status_ == kExit) {
				break;
			}
		}

		{
			std::lock_guard<std::mutex> lock(internel->connections_mutex_);
			for (auto &iter : internel->connections_) {
				iter.second->kcp_update();
			}
		}

		std::this_thread::sleep_for(kUCPDefaultInterval);
	}
}

Status ServerInternel::tranfer_status_from_init(
	std::shared_ptr<Sock> sock, std::shared_ptr<ServerInternel> internel)
{
	return kInit;
}

Status ServerInternel::tranfer_status_from_listen(
	std::shared_ptr<Sock> sock, std::shared_ptr<ServerInternel> internel)
{
	Message msg;
	std::string address;
	ssize_t ret = sock->recv_from(&msg, sizeof(msg), address);

	if (ret == -1) {
		return kExit;
	}

	if (ret == 0) { // no data
		return kListen;
	}

	if (ret != sizeof(msg)) { // error
		return kExit;
	}

	std::lock_guard<std::mutex> lock(internel->connections_mutex_);
	auto session = internel->connections_.find(address);
	if (msg.msg_type == kTypeNewSession) {
		Message msg;
		msg.msg_type = kTypeAcceptSession;
		msg.session_id = 0;
		msg.msg_size = 0;

		if (session != internel->connections_.end()) {
			msg.session_id = session->second->session_id();
		} else {
			auto connection = std::make_shared<ServerConnection>(sock, address);
			msg.session_id = connection->session_id();
			internel->connections_.insert(
				std::make_pair(address, connection));
		}
		sock->send_to(&msg, sizeof(msg), address);
	} else if (msg.msg_type == kTypeCloseSession) {
		if (session != internel->connections_.end()) {
			session->second->status(kClosed);
			internel->connections_.erase(session);
		}
	} else if (msg.msg_type == kTypeData) {
		if (session != internel->connections_.end()) {
			session->second->kcp_intput(msg.msg_data, msg.msg_size);
		}
	} else {
		return kExit;
	}

	return kListen;
}

void ServerInternel::exit()
{
	std::lock_guard<std::mutex> lock(status_mutex_);
	status_ = kExit;
}

uint32_t ServerConnection::session_id_counter_ = 0;

int ServerConnection::kcp_output(const char *buf, int len, ikcpcb *kcp,
								 void *user)
{
	ServerConnection *connection = (ServerConnection *)user;

	Message msg;
	msg.msg_type = kTypeData;
	msg.session_id = connection->session_id();
	msg.msg_size = len;
	memcpy(msg.msg_data, buf, len);

	return connection->sock_->send_to(&msg, sizeof(msg),
									  connection->remote_address_);
}

ServerConnection::ServerConnection(std::shared_ptr<Sock> sock,
								   const std::string &address)
	: sock_(sock)
	, remote_address_(address)
	, status_(kHandshake)
	, session_id_(++session_id_counter_)
{
	kcp_ = ikcp_create(session_id_, this);
	ikcp_setoutput(kcp_, kcp_output);
	ikcp_nodelay(kcp_, 1, 10, 2, 1);
	ikcp_wndsize(kcp_, 128, 128);
	ikcp_setmtu(kcp_, 1400);
	ikcp_update(kcp_, iclock());
}

ServerConnection::~ServerConnection()
{
	ikcp_release(kcp_);
}

ssize_t ServerConnection::send(const void *data, size_t size)
{
	std::lock_guard<std::mutex> lock(status_mutex_);
	if (status_ != kConnected) {
		return -1;
	}
	return ikcp_send(kcp_, (const char *)data, size);
}

ssize_t ServerConnection::recv(void *data, size_t size)
{
	std::lock_guard<std::mutex> lock(status_mutex_);
	if (status_ != kConnected) {
		return -1;
	}

	int ret = ikcp_recv(kcp_, (char *)data, size);

	if (ret < 0) {
		return 0;
	}

	return ret;
}

int ServerConnection::kcp_intput(const void *data, size_t size)
{
	return ikcp_input(kcp_, (const char *)data, size);
}

void ServerConnection::kcp_update()
{
	ikcp_update(kcp_, iclock());
}

uint32_t ServerConnection::session_id()
{
	return session_id_;
}

Status ServerConnection::status()
{
	std::lock_guard<std::mutex> lock(status_mutex_);
	Status ret = status_;
	return ret;
}

bool ServerConnection::status(Status new_status)
{
	std::lock_guard<std::mutex> lock(status_mutex_);
	status_ = new_status;
	return true;
}

void ServerConnection::close()
{
	std::lock_guard<std::mutex> lock(status_mutex_);
	if (status_ != kConnected) {
		return;
	}

	status_ = kClosed;

	Message msg;
	msg.msg_type = kTypeCloseSession;
	msg.session_id = session_id_;
	msg.msg_size = 0;

	sock_->send_to(&msg, sizeof(msg), remote_address_);

	// do not close socket
}

std::string ServerConnection::address()
{
	return remote_address_;
}
