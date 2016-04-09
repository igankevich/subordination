#ifndef APPS_DISCOVERY_SECRET_AGENT_HH
#define APPS_DISCOVERY_SECRET_AGENT_HH

/**
An agent that returns to its principal only when the node where it was sent
fails, or in case of network failure.
*/
struct Secret_agent: public Priority_kernel<Kernel> {

	typedef stdx::log<Secret_agent> this_log;

	void
	act() override {
		this_log() << "is deployed" << std::endl;
		delete this;
	}

	const Type<Kernel>
	type() const noexcept override {
		return static_type();
	}

	static const Type<Kernel>
	static_type() noexcept {
		return Type<Kernel>{
			112,
			"Secret_agent",
			[] (sys::packetstream& in) {
				Secret_agent* k = new Secret_agent;
				k->read(in);
				return k;
			}
		};
	}

};

#endif // APPS_DISCOVERY_SECRET_AGENT_HH
