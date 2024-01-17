#ifndef UCP_SRC_UCPBASE_HPP_
#define UCP_SRC_UCPBASE_HPP_

#include "kcp/ikcp.h"

#include <chrono>
#include <string>
#include <thread>

#include <cstdint>

namespace ucp {

enum MessageType : uint8_t {
	kTypeNewSession,
	kTypeAcceptSession,
	kTypeRejectSession,
	kTypeCloseSession,
	kTypeData,
	kHeartbeat,
};

enum Status {
	kInit,
	kHandshake,
	kListen,
	kConnected,
	kClosed,
	kExit,
};

struct Message {
	MessageType msg_type;
	uint32_t session_id;
	uint32_t msg_size;
	char msg_data[1024];
};

constexpr size_t kUCPMessageSize = sizeof(Message);

constexpr std::chrono::milliseconds kUCPDefaultInterval =
	std::chrono::milliseconds(10);

constexpr std::chrono::milliseconds kUCPDefaultHeartbeatTimeout =
	std::chrono::milliseconds(30000);

constexpr std::chrono::milliseconds kUCPDefaultHeartbeatInterval =
	std::chrono::milliseconds(10000);

constexpr std::chrono::milliseconds kUCPDefaultHandshakeTimeout =
	std::chrono::milliseconds(3000);

class Session {
public:
	virtual ~Session()
	{
	}

	virtual ssize_t send(const void *data, size_t size) = 0;

	virtual ssize_t recv(void *data, size_t size) = 0;
	virtual void close() = 0;

	/**
	 * @brief get address
	 * 
	 * @param address 
	 */
	virtual std::string address() = 0;
};

class Sock {
public:
	virtual ~Sock() = default;

	/**
	 * @brief bind to address
	 * 
	 * @param address if address is empty, bind to a valid address
	 * @return true 
	 * @return false 
	 */
	virtual bool bind(const std::string &address) = 0;

	/**
	 * @brief get address
	 * 
	 * @param address 
	 */
	virtual std::string address() = 0;

	/**
	 * @brief send a packet to address
	 * 
	 * @param data data to send
	 * @param size size of data
	 * @param to target address 
	 * @return ssize_t size of data sent, -1 if error
	 */
	virtual ssize_t send_to(const void *data, size_t size,
							const std::string &to) = 0;

	/**
	 * @brief recv a packet
	 * 
	 * @param data buffer to store data
	 * @param size size of buffer
	 * @param from source address
	 * @return ssize_t size of data received, -1 if error, 0 if no data
	 */
	virtual ssize_t recv_from(void *data, size_t size, std::string &from) = 0;

	/**
	 * @brief close socket
	 * 
	 */
	virtual void close() = 0;
};

static IUINT32 iclock()
{
	auto now_unix = std::chrono::system_clock::now();
	auto now_ms =
		std::chrono::time_point_cast<std::chrono::milliseconds>(now_unix)
			.time_since_epoch()
			.count();

	return (IUINT32)(now_ms & 0xfffffffful);
}

} // namespace ucp

#endif // UCP_SRC_UCPBASE_HPP_
