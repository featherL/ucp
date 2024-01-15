#include "../src/ucp.hpp"

#include <cstdio>
#include <cstdio>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <cstring>

static std::queue<std::string> server_msg;
static std::mutex server_mutex;

static std::queue<std::string> client_msg;
static std::mutex client_mutex;

class SimpleSock : public ucp::Sock {
public:
	bool bind(const std::string &address) override final
	{
		address_ = address;
		return true;
	}

    std::string address() override final
    {
        return address_;
    }

	ssize_t send_to(const void *data, size_t size,
					const std::string &to) override final
	{
		if (address_ == to) {
			return -1;
		}

		if (to == "server") {
			std::lock_guard<std::mutex> lock(server_mutex);
			server_msg.push(std::string((const char *)data, size));
			return size;
		} else if (to == "client") {
			std::lock_guard<std::mutex> lock(client_mutex);
			client_msg.push(std::string((const char *)data, size));
			return size;
		} else {
			return -1;
		}
	}

	ssize_t recv_from(void *data, size_t size, std::string &from) override final
	{
		if (address_ == "client") {
			from = "server";
			std::lock_guard<std::mutex> lock(client_mutex);
			if (client_msg.empty()) {
				return 0;
			}

			std::string msg = client_msg.front();
			client_msg.pop();

			size_t copy_size = std::min(size, msg.size());

			memcpy(data, msg.data(), copy_size);
			return copy_size;
		} else if (address_ == "server") {
			from = "client";
			std::lock_guard<std::mutex> lock(server_mutex);
			if (server_msg.empty()) {
				return 0;
			}

			std::string msg = server_msg.front();
			server_msg.pop();

			size_t copy_size = std::min(size, msg.size());

			memcpy(data, msg.data(), copy_size);
			return copy_size;
		} else {
			return -1;
		}
	}

	void close() override final
	{
		return;
	}

private:
	std::string address_;
};

void server_thread_func()
{
    ucp::Server<SimpleSock> server;
    server.listen_at("server");
    fprintf(stderr, "server listen at server\n");

    auto session = server.accept();
    fprintf(stderr, "server accept\n");

    char buf[1024];
    while (true) {
        ssize_t recv_size = session->recv(buf, 1024);
        if (recv_size > 0) {
            buf[recv_size] = '\0';
            printf("server recv: %s\n", buf);
            session->send(buf, recv_size);
        } else if (recv_size == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else {
            fprintf(stderr, "server recv error\n");
            break;
        }
    }

    fprintf(stderr, "session close\n");

    session->close();
}

int main()
{
	std::thread server_thread(server_thread_func);
    ucp::Client<SimpleSock> client;
    
    if (!client.bind("client")) {
        printf("client bind error\n");
        return -1;
    }
    fprintf(stderr, "client bind at client\n");

    if (!client.connect("server")) {
        printf("client connect error\n");
        return -1;
    }
    fprintf(stderr, "client connect to server\n");

    char buf[1024];
    int flag = 0;
    while (fgets(buf, 1024, stdin) && !flag) {
        size_t len = strlen(buf);
        if (len > 0) {
            buf[len - 1] = '\0';
        }

        ssize_t send_size = client.send(buf, len);
        if (send_size < 0) {
            printf("client send error\n");
            break;
        }

        while (true) {
            ssize_t recv_size = client.recv(buf, 1024-1);
            if (recv_size > 0) {
                buf[recv_size] = '\0';
                printf("client recv: %s\n", buf);
                break;
            } else if (recv_size == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                fprintf(stderr, "client recv error\n");
                flag = 1;
            }
        }
    }

    client.close();
    server_thread.join();

	return 0;
}