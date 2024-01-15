#ifndef UCP_SRC_UCPSERVER_HPP_
#define UCP_SRC_UCPSERVER_HPP_

#include <cstdint>
#include <cstdio>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <mutex>
#include <iostream>

#include "ucpbase.hpp"
#include "kcp/ikcp.h"

namespace ucp {

class ServerConnection : public Session {
private:
	static uint32_t session_id_counter_;

	static int kcp_output(const char *buf, int len, ikcpcb *kcp, void *user);

public:
	ServerConnection() = delete;
	ServerConnection(std::shared_ptr<Sock> sock, const std::string &address);

	~ServerConnection() override;

	ssize_t send(const void *data, size_t size) override;

	ssize_t recv(void *data, size_t size) override;
	void close() override;

	std::string address() override;

	int kcp_intput(const void *data, size_t size);
	void kcp_update();

	uint32_t session_id();
	Status status();
	bool status(Status new_status);

private:
	ikcpcb *kcp_;
	std::shared_ptr<Sock> sock_;
	std::string remote_address_;
	uint32_t session_id_;

	std::mutex status_mutex_;
	Status status_;
};

class ServerInternel {
public:
	static void monitor_thread_func(std::shared_ptr<Sock> sock,
									std::shared_ptr<ServerInternel> internel);
	static Status
	tranfer_status_from_init(std::shared_ptr<Sock> sock,
							 std::shared_ptr<ServerInternel> internel);
	static Status
	tranfer_status_from_listen(std::shared_ptr<Sock> sock,
							   std::shared_ptr<ServerInternel> internel);

	ServerInternel() = default;
	~ServerInternel() = default;

	bool status(Status new_status);

	void exit();

	std::mutex status_mutex_;
	Status status_;

	std::mutex connections_mutex_;
	std::unordered_map<std::string, std::shared_ptr<ServerConnection> >
		connections_;
};

template <class T>
class Server {
public:
	Server<T>()
		: sock_(std::make_shared<T>())
		, internel_(std::make_shared<ServerInternel>())
	{
		monitor_thread_ =
			std::thread(ServerInternel::monitor_thread_func, sock_, internel_);
	}

	~Server<T>()
	{
		internel_->exit();
		monitor_thread_.join();
	}

	/**
	 * @brief listen at address
	 * 
	 * @param address
	 * @return true 
	 * @return false 
	 */
	bool listen_at(const std::string &address)
	{
		std::lock_guard<std::mutex> lock(internel_->status_mutex_);
		if (internel_->status_ != kInit) {
			return false;
		}

		internel_->status_ = kListen;

		return sock_->bind(address);
	}

	/**
	 * @brief accept a new connection
	 * 
	 * @return std::shared_ptr<Session> 
	 */
	std::shared_ptr<Session> accept()
	{
		while (true) {
			{
				std::lock_guard<std::mutex> lock(internel_->status_mutex_);
				if (internel_->status_ != kListen) {
					return nullptr;
				}
			}

			{
				std::lock_guard<std::mutex> lock(internel_->connections_mutex_);
				for (auto &iter : internel_->connections_) {
					if (iter.second->status() == kHandshake) {
						iter.second->status(kConnected);
						return iter.second;
					}
				}
			}

			std::this_thread::sleep_for(kUCPDefaultInterval);
		}
	}

private:
	std::shared_ptr<T> sock_;
	std::thread monitor_thread_;
	std::shared_ptr<ServerInternel> internel_;
};

} // namespace ucp

#endif // UCP_SRC_UCPSERVER_HPP_