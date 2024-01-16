#ifndef UCP_SRC_UCPUDP_HPP_
#define UCP_SRC_UCPUDP_HPP_

// only support linux
#include <string>
#ifndef __linux__
#error "only support linux"
#endif

#include "ucpbase.hpp"
#include <arpa/inet.h>
#include <unistd.h>
#include <stdexcept>
#include <fcntl.h>

namespace ucp {
class UDPSock : public Sock {
public:
	static bool str_to_sockaddr(const std::string &address,
								struct sockaddr_in &addr)
	{
		// address is ip:port
		auto pos = address.find(':');
		if (pos == std::string::npos) {
			return false;
		}

		addr.sin_family = AF_INET;
		addr.sin_port = htons(std::stoi(address.substr(pos + 1)));
		addr.sin_addr.s_addr = inet_addr(address.substr(0, pos).c_str());
		return true;
	}

	static bool sockaddr_to_str(const struct sockaddr_in &addr,
								std::string &address)
	{
		char buf[32];
		if (inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf)) == nullptr) {
			return false;
		}

		address = std::string(buf) + ":" + std::to_string(ntohs(addr.sin_port));
		return true;
	}

public:
	UDPSock()
	{
		fd_ = socket(AF_INET, SOCK_DGRAM, 0);
		if (fd_ == -1) {
			throw std::runtime_error("create socket failed");
		}

		// set non-blocking
		int flags = fcntl(fd_, F_GETFL, 0);
		if (flags == -1) {
			throw std::runtime_error("fcntl failed");
		}

		if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
			throw std::runtime_error("fcntl failed");
		}
	}

	~UDPSock()
	{
		close();
	}

	bool bind(const std::string &address) override
	{
		if (address == "") {
			return true;
		}

		struct sockaddr_in addr;
		if (!str_to_sockaddr(address, addr)) {
			return false;
		}

		if (::bind(fd_, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
			return false;
		}

		return true;
	}

	std::string address() override
	{
		struct sockaddr_in addr;
		socklen_t addr_len = sizeof(addr);
		if (getsockname(fd_, (struct sockaddr *)&addr, &addr_len) == -1) {
			return "";
		}

		std::string address;
		return sockaddr_to_str(addr, address) ? address : "";
	}

	ssize_t send_to(const void *data, size_t size,
					const std::string &address) override
	{
		if (fd_ == -1) {
			return -1;
		}

		struct sockaddr_in addr;
		if (!str_to_sockaddr(address, addr)) {
			return -1;
		}

		return sendto(fd_, data, size, 0, (struct sockaddr *)&addr,
					  sizeof(addr));
	}

	ssize_t recv_from(void *data, size_t size, std::string &address) override
	{
		if (fd_ == -1) {
			return -1;
		}

		struct sockaddr_in addr;
		socklen_t addr_len = sizeof(addr);
		ssize_t ret =
			recvfrom(fd_, data, size, 0, (struct sockaddr *)&addr, &addr_len);

		if (ret < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return 0;
			}
		}

		if (ret > 0) {
			sockaddr_to_str(addr, address);
		}

		return ret;
	}

	void close() override
	{
		if (fd_ != -1) {
			::close(fd_);
			fd_ = -1;
		}
	}

private:
	int fd_;
};

};

#endif // UCP_SRC_UCPUDP_HPP_