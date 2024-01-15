#ifndef UCP_SRC_UCPCLIENT_HPP_
#define UCP_SRC_UCPCLIENT_HPP_

#include <map>
#include <memory>
#include <cstdint>
#include <mutex>
#include <string>
#include <sys/types.h>
#include <thread>

#include "ucpbase.hpp"
#include "kcp/ikcp.h"

namespace ucp {

class ClientInternel {
public:
	static void monitor_thread_func(std::shared_ptr<ClientInternel> internel);

private:
	static int ucp_output(const char *buf, int len, ikcpcb *kcp, void *user);
	static Status
	tranfer_status_from_init(std::shared_ptr<ClientInternel> internel);
	static Status
	tranfer_status_from_handshake(std::shared_ptr<ClientInternel> internel);
	static Status
	tranfer_status_from_connected(std::shared_ptr<ClientInternel> internel);

public:
	ClientInternel() = delete;
	ClientInternel(std::shared_ptr<Sock> sock);
	~ClientInternel();

	bool bind(const std::string &address);
	bool connect(const std::string &address);
	ssize_t send(const void *data, size_t size);
	ssize_t recv(void *data, size_t size);
	void close();

private:
	bool wait_for_accept_();

private:
	std::shared_ptr<Sock> sock_;
	Status status_;
	std::mutex status_mutex_;

	std::string local_address_;
	std::string remote_address_;
	uint32_t session_id_;
	ikcpcb *kcp_;
};

template <class T>
class Client : public Session {
public:
	Client()
		: internel_(std::make_shared<ClientInternel>(std::make_shared<T>()))
	{
		monitor_thread_ =
			std::thread(ClientInternel::monitor_thread_func, internel_);
	}

	~Client() override
	{
		internel_->close();
		monitor_thread_.join();
	}

	/**
	 * @brief bind at address
	 * 
	 * @param address 
	 * @return true 
	 * @return false 
	 */
	bool bind(const std::string &address)
	{
		return internel_->bind(address);
	}

	/**
	 * @brief connect to address
	 * 
	 * @param address 
	 * @return true 
	 * @return false 
	 */
	bool connect(const std::string &address)
	{
		return internel_->connect(address);
	}

	/**
	 * @brief send data to server
	 * 
	 * @param data data to send
	 * @param size size of data
	 * @return ssize_t size of data sent, -1 if error
	 */
	ssize_t send(const void *data, size_t size) override
	{
		return internel_->send(data, size);
	}

	/**
	 * @brief recv data from server
	 * 
	 * @param data buffer to store data
	 * @param size size of buffer
	 * @return ssize_t size of data received, -1 if error, 0 if no data
	 */
	ssize_t recv(void *data, size_t size) override
	{
		return internel_->recv(data, size);
	}

	/**
	 * @brief close session
	 * 
	 */
	void close() override
	{
		internel_->close();
	}

private:
	std::shared_ptr<ClientInternel> internel_;
	std::thread monitor_thread_;
};

} // namespace ucp

#endif // UCP_SRC_UCPCLIENT_HPP_