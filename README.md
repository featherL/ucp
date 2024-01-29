# ucp
Protocol provides reliable connection

**refer**: https://github.com/skywind3000/kcp

## Usage

implement ucp::Sock
```c++
class MySock : public ucp::Sock {
public:
/**
	 * @brief bind to address
	 * 
	 * @param address if address is empty, bind to a valid address
	 * @return true 
	 * @return false 
	 */
	bool bind(const std::string &address) override
	{
		// TODO
	}

	/**
	 * @brief get address
	 * 
	 * @param address 
	 */
	std::string address() override
	{
		// TODO
	}

	/**
	 * @brief send a packet to address
	 * 
	 * @param data data to send
	 * @param size size of data
	 * @param to target address 
	 * @return ssize_t size of data sent, -1 if error
	 */
	ssize_t send_to(const void *data, size_t size,
							const std::string &to) override
	{
		// TODO
	}

	/**
	 * @brief recv a packet
	 * 
	 * @param data buffer to store data
	 * @param size size of buffer
	 * @param from source address
	 * @return ssize_t size of data received, -1 if error, 0 if no data
	 */
	size_t recv_from(void *data, size_t size, std::string &from) override
	{
		// TODO
	}

	/**
	 * @brief close socket
	 * 
	 */
	void close() override
	{
		// TODO
	}

};
```


**Server**
```c++
ucp::Server<MySock> server;
server.listen_at("<address>");

char buff[1024];
while (true) {
	auto session = server.accept();

	while (true) {
		ssize_t recv_size = session->recv(buf, 1024-1);
		if (recv_size > 0) {
				buf[recv_size] = '\0';
				printf("server recv: %s\n", buf);
				session->send(buf, recv_size);
		} else if (recv_size == 0) { // no data, just wait
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
		} else {
			fprintf(stderr, "server recv error\n");
			break;
		}
	}

	session->close()
}
```

**Client**
```c++
ucp::Client<MySock> client;
client.bind("<client addr>");
client.connect("<server addr>");

char buff[] = "hello server";
client.send(buff, sizeof(buff));

```

see more: [examples](./examples)
