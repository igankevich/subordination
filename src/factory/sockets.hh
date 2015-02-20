namespace factory {

	struct Server_socket: public Socket {
		Server_socket() {}
		explicit Server_socket(const Socket& rhs): Socket(rhs) {}
		explicit Server_socket(const Endpoint& endp, Socket_type type = RELIABLE_SOCKET) {
			this->listen(endp, type);
		}
//		Server_socket(const std::string& host, Port port) { this->listen(host, port); }
		~Server_socket() { this->close(); }
	};

	struct Client_socket: public Socket {
		Client_socket(Endpoint endpoint, Socket_type type = RELIABLE_SOCKET) {
			this->connect(endpoint, type);
		}
//		Client_socket(const std::string& host, Port port) { this->connect(host, port); }
		virtual ~Client_socket() {
//			std::clog << "~Client_socket()" << std::endl;
			this->close();
		}
	};

	struct Broadcast_socket: public Client_socket {
		Broadcast_socket(Endpoint endp): Client_socket(endp, UNRELIABLE_SOCKET) {
			this->options(SO_BROADCAST);
		}
	};

}
