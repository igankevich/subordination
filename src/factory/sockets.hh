namespace factory {

	struct Server_socket: public Socket {
		Server_socket() {}
		explicit Server_socket(const Socket& rhs): Socket(rhs) {}
		explicit Server_socket(const Endpoint& endp) { this->listen(endp.host(), endp.port()); }
		Server_socket(const std::string& host, Port port) { this->listen(host, port); }
		~Server_socket() { this->close(); }
	};

	struct Client_socket: public Socket {
		Client_socket(const std::string& host, Port port) { this->connect(host, port); }
		~Client_socket() {
//			std::clog << "~Client_socket()" << std::endl;
			this->close();
		}
	};

}
