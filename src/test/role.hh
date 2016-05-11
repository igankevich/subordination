#ifndef TEST_ROLE_HH
#define TEST_ROLE_HH

namespace test {

	enum struct Role {
		Master,
		Slave
	};

	const char*
	to_string(Role rhs) {
		switch (rhs) {
			case Role::Master: return "master";
			case Role::Slave: return "slave";
			default: return "unknown";
		}
	}

	std::ostream&
	operator<<(std::ostream& out, const Role& rhs) {
		return out << to_string(rhs);
	}

	std::istream&
	operator>>(std::istream& in, Role& rhs) {
		std::string tmp;
		in >> tmp;
		if (tmp == to_string(Role::Master)) {
			rhs = Role::Master;
		} else
		if (tmp == to_string(Role::Slave)) {
			rhs = Role::Slave;
		}
		return in;
	}

}

#endif // TEST_ROLE_HH
