#include "../src/ucp.hpp"
#include "../src/ucpudp.hpp"
#include <cstdio>
#include <memory>

void client_handler(std::shared_ptr<ucp::Session> session)
{
	char buf[1024];
	ssize_t nread = 0;

	while (true) {
		nread = session->recv(buf, sizeof(buf));
		if (nread < 0) {
			std::cerr << "recv failed" << std::endl;
			return;
		}

		if (nread > 0) {
			std::cout << "from client " << session->address() << ": "
					  << std::string(buf, nread) << std::endl;
			ssize_t n = session->send(buf, nread);
			if (n < 0) {
				std::cerr << "send failed" << std::endl;
				return;
			}
		}

		std::this_thread::sleep_for(
			std::chrono::milliseconds(10)); // wait for data
	}
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <ip> <port>" << std::endl;
		return 1;
	}

	ucp::Server<ucp::UDPSock> server;
	std::string address = std::string(argv[1]) + ":" + std::string(argv[2]);
	if (!server.listen_at(address)) {
		std::cerr << "listen on " << address << " failed" << std::endl;
		return 1;
	}

	while (true) {
		auto session = server.accept();
		if (session == nullptr) {
			std::cerr << "accept failed" << std::endl;
			return 1;
		}
		std::cerr << "accept new session " << session->address() << std::endl;

		std::thread t(client_handler, session);
		t.detach();
	}

	return 0;
}