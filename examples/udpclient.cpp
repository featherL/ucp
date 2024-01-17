#include "../src/ucp.hpp"
#include "../src/ucpudp.hpp"

#include <cstddef>
#include <iostream>

int main(int argc, char *argv[])
{
	if (argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <ip> <port>" << std::endl;
		return 1;
	}

	ucp::Client<ucp::UDPSock> client;

	std::string remote_addr = std::string(argv[1]) + ":" + std::string(argv[2]);
	if (!client.connect(remote_addr)) {
		std::cerr << "connect to " << remote_addr << " failed" << std::endl;
		return 1;
	}

	std::cout << "connect to " << remote_addr << " success" << std::endl;

	std::string msg;
	while (std::getline(std::cin, msg)) {
		ssize_t n = client.send(msg.data(), msg.size());
		if (n < 0) {
			std::cerr << "send failed" << std::endl;
			return 1;
		}

		if (n > 0) {
			ssize_t nread = 0;
			char buf[1024];
			while (nread == 0) {
				nread = client.recv(buf, sizeof(buf));
				if (nread < 0) {
					std::cerr << "recv failed" << std::endl;
					return 1;
				}

				if (nread > 0) {
					std::cout << "from server: " << std::string(buf, nread)
							  << std::endl;
					break;
				}

				std::this_thread::sleep_for(
					std::chrono::milliseconds(10)); // wait for data
			}
		} else {
			std::cout << "send 0" << std::endl;
		}
	}

	client.close();

	return 0;
}