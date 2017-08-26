#ifndef APPS_DISCOVERY_SECRET_AGENT_HH
#define APPS_DISCOVERY_SECRET_AGENT_HH

/**
An agent that returns to its principal only when the node where it was sent
fails, or in case of network failure.
*/
struct Secret_agent: public Priority_kernel<Kernel> {

	void
	act() override {
		delete this;
	}

};

#endif // vim:filetype=cpp
