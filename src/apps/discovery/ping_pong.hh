#ifndef APPS_DISCOVERY_PING_PONG_HH
#define APPS_DISCOVERY_PING_PONG_HH

#include <factory/factory.hh>

struct Ping: public Kernel {

	typedef stdx::log<Ping> this_log;

	Ping(): _data() {}

	explicit
	Ping(int x): _data(x) {
	}

	void act() override {
		this_log() << "act()" << std::endl;
		commit(remote_server, this);
	}

	void write(sys::packetstream& out) override {
		Kernel::write(out);
		out << _data;
	}

	void read(sys::packetstream& in) override {
		this_log() << "Ping::read()" << std::endl;
		Kernel::read(in);
		in >> _data;
	}

	int
	get_x() const noexcept {
		return _data;
	}

private:

	int _data;

};

struct Ping_pong: public Kernel {

	typedef stdx::log<Ping_pong> this_log;

	Ping_pong() = default;

	explicit
	Ping_pong(int n):
	_numkernels(n)
	{}

	void
	act() override {
		this_log() << "sending ping #" << _currentkernel + 1 << std::endl;
		int x = 1;
		_expectedsum += x;
		factory::upstream(remote_server, this, new Ping(x));
		if (++_currentkernel < _numkernels) {
			this->after(std::chrono::seconds(1));
			factory::schedule(this);
		} else {
			this_log() << "finished sending pings" << std::endl;
		}
	}

	void
	react(Kernel* child) override {
		Ping* ping = dynamic_cast<Ping*>(child);
		this_log() << "ping returned from " << ping->from() << " with " << ping->result() << std::endl;
		_realsum += ping->get_x();
		if (
			not _some_kernels_came_from_a_remote_server
			and not ping->from()
		) {
			_some_kernels_came_from_a_remote_server = true;
		}
		if (++_numreceived == _numkernels) {
			this_log() << stdx::make_fields(
				"num_kernels", _numkernels,
				"expected_sum", _expectedsum,
				"real_sum", _realsum,
				"representative", _some_kernels_came_from_a_remote_server
			) << std::endl;
			bool success = _some_kernels_came_from_a_remote_server and _realsum == _expectedsum;
			factory::commit(this, success ? Result::success : Result::error);
		}
	}

private:

	int _expectedsum = 0;
	int _realsum = 0;
	int _numkernels = 10;
	int _numreceived = 0;
	int _currentkernel = 0;
	bool _some_kernels_came_from_a_remote_server = false;

};

#endif // APPS_DISCOVERY_PING_PONG_HH
