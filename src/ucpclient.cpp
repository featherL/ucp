#include "ucpclient.hpp"

#include "ucpbase.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#include "kcp/ikcp.h"
#include "ucp.hpp"

using namespace ucp;

void ClientInternel::monitor_thread_func(
	std::shared_ptr<ClientInternel> internel)
{
	while (true) {
		{
			std::lock_guard<std::mutex> lock(internel->status_mutex_);

			if (internel->status_ == kInit) {
				internel->status_ = tranfer_status_from_init(internel);
			} else if (internel->status_ == kHandshake) {
				internel->status_ = tranfer_status_from_handshake(internel);
			} else if (internel->status_ == kConnected) {
				ikcp_update(internel->kcp_, iclock());
				internel->status_ = tranfer_status_from_connected(internel);
			} else if (internel->status_ == kClosed) {
				break;
			} else if (internel->status_ == kExit) {
				break;
			}
		}

		std::this_thread::sleep_for(kUCPDefaultInterval);
	}
}

Status ClientInternel::tranfer_status_from_init(
	std::shared_ptr<ClientInternel> internel)
{
	return kInit;
}

Status ClientInternel::tranfer_status_from_handshake(
	std::shared_ptr<ClientInternel> internel)
{
	Message msg;
	std::string address;
	ssize_t ret = internel->sock_->recv_from(&msg, sizeof(msg), address);

	if (ret == -1) {
		return kExit;
	}

	if (ret == 0) {
		return kHandshake;
	}

	if (ret != sizeof(msg)) {
		return kExit;
	}

	if (address != internel->remote_address_) {
		return kExit;
	}

	if (msg.msg_type == kTypeAcceptSession) {
		internel->session_id_ = msg.session_id;
		internel->kcp_ = ikcp_create(msg.session_id, internel.get());
		ikcp_setoutput(internel->kcp_, ucp_output);
		ikcp_nodelay(internel->kcp_, 1, 10, 2, 1);
		ikcp_wndsize(internel->kcp_, 128, 128);
		ikcp_setmtu(internel->kcp_, 1400);
		ikcp_update(internel->kcp_, iclock());

		return kConnected;
	} else if (msg.msg_type == kTypeRejectSession) {
		return kInit;
	}

	return kExit;
}

Status ClientInternel::tranfer_status_from_connected(
	std::shared_ptr<ClientInternel> internel)
{
	Message msg;
	std::string address;
	ssize_t ret = internel->sock_->recv_from(&msg, sizeof(msg), address);
	if (ret == -1) {
		return kExit;
	}

	if (ret == 0) {
		return kConnected;
	}

	if (ret != sizeof(msg)) {
		return kExit;
	}

	if (address != internel->remote_address_) {
		return kExit;
	}

	if (msg.msg_type == kTypeCloseSession) {
		return kClosed;
	}

	if (msg.msg_type == kTypeData) {
		ikcp_input(internel->kcp_, msg.msg_data, msg.msg_size);
		return kConnected;
	}

	return kExit;
}

int ClientInternel::ucp_output(const char *buf, int len, ikcpcb *kcp,
							   void *user)
{
	ClientInternel *internel = (ClientInternel *)user;

	Message msg;
	msg.msg_type = kTypeData;
	msg.session_id = internel->session_id_;
	msg.msg_size = len;
	memcpy(msg.msg_data, buf, len);

	return internel->sock_->send_to(&msg, sizeof(msg),
									internel->remote_address_);
}

ClientInternel::ClientInternel(std::shared_ptr<Sock> sock)
	: sock_(sock)
	, status_(kInit)
	, kcp_(nullptr)
{
	local_address_.clear();
	remote_address_.clear();
	session_id_ = 0;
}

ClientInternel::~ClientInternel()
{
	if (kcp_ != nullptr) {
		ikcp_release(kcp_);
	}
}

bool ClientInternel::bind(const std::string &address)
{
	std::lock_guard<std::mutex> lock(status_mutex_);
	if (status_ != kInit) {
		return false;
	}

	if (!sock_->bind(address))
		return false;

	local_address_ = sock_->address();
	return true;
}

bool ClientInternel::connect(const std::string &address)
{
	if (local_address_ == "" && !bind("")) {
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(status_mutex_);
		if (status_ != kInit) {
			return false;
		}

		remote_address_ = address;
		status_ = kHandshake;
	} // free lock

	auto start = std::chrono::steady_clock::now();
	do {
		if (std::chrono::steady_clock::now() - start >
			kUCPDefaultHandshakeTimeout) {
			return false;
		}

		Message msg;
		msg.msg_type = kTypeNewSession;
		msg.session_id = 0;
		msg.msg_size = 0;

		if (sock_->send_to(&msg, sizeof(msg), remote_address_) == -1) {
			return false;
		}

	} while (!wait_for_accept_with_timeout_(kUCPDefaultTimeout));



	return true;
}

bool ClientInternel::wait_for_accept_()
{
	while (true) {
		{
			std::lock_guard<std::mutex> lock(status_mutex_);
			if (status_ == kConnected) {
				return true;
			} else if (status_ != kHandshake) {
				return false;
			}
		}

		std::this_thread::sleep_for(kUCPDefaultInterval);
	}
}

bool ClientInternel::wait_for_accept_with_timeout_(std::chrono::milliseconds timeout)
{
	auto start = std::chrono::steady_clock::now();
	while (true) {
		{
			std::lock_guard<std::mutex> lock(status_mutex_);
			if (status_ == kConnected) {
				return true;
			} else if (status_ != kHandshake) {
				return false;
			}
		}

		auto now = std::chrono::steady_clock::now();
		if (now - start > timeout) {
			return false;
		}

		std::this_thread::sleep_for(kUCPDefaultInterval);
	}
}

ssize_t ClientInternel::send(const void *data, size_t size)
{
	std::lock_guard<std::mutex> lock(status_mutex_);
	if (status_ != kConnected) {
		return -1;
	}

	int ret = ikcp_send(kcp_, (const char *)data, size);
	return ret < 0 ? -1 : ret;
}

ssize_t ClientInternel::recv(void *data, size_t size)
{
	std::lock_guard<std::mutex> lock(status_mutex_);
	if (status_ != kConnected) {
		return -1;
	}

	int ret = ikcp_recv(kcp_, (char *)data, size);

	return ret < 0 ? 0 : ret;
}

void ClientInternel::close()
{
	std::lock_guard<std::mutex> lock(status_mutex_);
	if (status_ != kConnected) {
		return;
	}
	
	Message msg;
	msg.msg_type = kTypeCloseSession;
	msg.session_id = session_id_;
	msg.msg_size = 0;

	sock_->send_to(&msg, sizeof(msg), remote_address_);
	ikcp_update(kcp_, iclock());

	status_ = kClosed;

	sock_->close();
}

std::string ClientInternel::address()
{
	return local_address_;
}
