#include <fstream>

#include <factory/factory.hh>
#include <factory/server/cpu_server.hh>
#include <factory/server/timer_server.hh>
#include <factory/server/nic_server.hh>

// disable logs
namespace stdx {

	template<>
	struct disable_log_category<sys::buffer_category>:
	public std::true_type {};

	template<>
	struct disable_log_category<factory::components::kernel_category>:
	public std::true_type {};

	template<>
	struct disable_log_category<factory::components::server_category>:
	public std::true_type {};

}


namespace factory {
	inline namespace this_config {

		struct config {
			typedef components::Server<config> server;
			typedef components::Principal<config> kernel;
			typedef components::CPU_server<config> local_server;
			typedef components::NIC_server<config, sys::socket> remote_server;
			typedef components::Timer_server<config> timer_server;
			typedef components::No_server<config> app_server;
			typedef components::Basic_factory<config> factory;
		};

		typedef config::kernel Kernel;
		typedef config::server Server;
	}
}

using namespace factory;
using namespace factory::this_config;


#include <zlib.h>
using namespace factory;

typedef int32_t Year;
typedef int32_t Month;
typedef int32_t Day;
typedef int32_t Hour;
typedef int32_t Minute;
typedef int32_t Station;
typedef char Variable;

typedef std::string Resource;

struct Date {

	Date() {}
	Date(const Date& rhs):
		_year(rhs._year),
		_month(rhs._month),
		_day(rhs._day),
		_hours(rhs._hours),
		_minutes(rhs._minutes)
	{}

	Date& operator=(const Date& rhs) {
		_year = rhs._year;
		_month = rhs._month;
		_day = rhs._day;
		_hours = rhs._hours;
		_minutes = rhs._minutes;
		return *this;
	}

	bool operator==(const Date& rhs) const {
		return _year == rhs._year &&
		_month       == rhs._month &&
		_day         == rhs._day &&
		_hours       == rhs._hours &&
		_minutes     == rhs._minutes;
	}

	bool operator<(const Date& rhs) const {
		return integral() < rhs.integral();
	}

	int64_t integral() const {
		return _minutes + 60*_hours + 60*24*_day + 60*24*31*_month + 60*24*31*12*_year;
	}

	friend std::istream& operator>>(std::istream& in, Date& rhs) {
		return in >> rhs._year >> rhs._month >> rhs._day >> rhs._hours >> rhs._minutes;
	}

	friend std::ostream& operator<<(std::ostream& out, const Date& rhs) {
		return out << std::setfill('0')
			                << rhs._year << ' '
			<< std::setw(2) << rhs._month << ' '
			<< std::setw(2) << rhs._day << ' '
			<< std::setw(2) << rhs._hours << ' '
			<< std::setw(2) << rhs._minutes;
	}

private:
	Year _year;
	Month _month;
	Day _day;
	Hour _hours;
	Minute _minutes;
};

/*

41013d2010.txt.gz
41013i2010.txt.gz
41013j2010.txt.gz
41013k2010.txt.gz
41013w2010.txt.gz
42001d2010.txt.gz
42001i2010.txt.gz
42001j2010.txt.gz
42001k2010.txt.gz
42001w2010.txt.gz

*/

struct Observation {

	Observation() {}

	Observation(Year year, Station station, Variable var):
		_year(year), _station(station), _var(var) {}

	Observation(const Observation& rhs):
		_year(rhs._year), _station(rhs._station), _var(rhs._var) {}

	Year year() const { return _year; }
	Station station() const { return _station; }
	Variable variable() const { return _var; }

	std::string filename() const {
		std::stringstream str;
		str << '.'
			<< '/' << year()
			<< '/' << station()
			<< '/' << station() << variable() << year()
			<< ".txt.gz";
		return str.str();
	}

	friend std::istream& operator>>(std::istream& in, Observation& rhs) {
		return in >> rhs._station >> rhs._var >> rhs._year;
	}

	friend std::ostream& operator<<(std::ostream& out, const Observation& rhs) {
		return out << rhs._year << ',' << rhs._station<< ',' << rhs._var;
	}

	friend sys::packetstream& operator>>(sys::packetstream& in, Observation& rhs) {
		return in >> rhs._year >> rhs._station >> rhs._var;
	}

	friend sys::packetstream& operator<<(sys::packetstream& out, const Observation& rhs) {
		return out << rhs._year << rhs._station << rhs._var;
	}

private:
	Year _year;
	Station _station;
	Variable _var;
};

struct Spectrum_kernel: public Kernel {

	enum {
		DENSITY = 'w',
		ALPHA_1 = 'd',
		ALPHA_2 = 'i',
		R_1     = 'j',
		R_2     = 'k'
	};

	typedef std::unordered_map<Variable, std::vector<float>> Map;

	Spectrum_kernel(Map& m, Date d, const std::vector<float>& freq):
		_data(m), _date(d), _frequencies(freq), _variance(0)
	{}

	void act() override {
		// TODO Filter 999 values.
		_variance = compute_variance();
		commit(local_server());
	}

	float spectrum(int32_t i, float angle) {
		return _data[DENSITY][i] * (1.0f/PI) * (0.5f
			+ 0.01f*_data[R_1][i]*std::cos(      angle - _data[ALPHA_1][i])
			+ 0.01f*_data[R_2][i]*std::cos(2.0f*(angle - _data[ALPHA_2][i])));
	}

	float compute_variance() {
		const float theta0 = 0;
		const float theta1 = 2.0f*PI;
		int32_t n = _frequencies.size();
		float sum = 0;
		for (int32_t i=0; i<n; ++i) {
			for (int32_t j=0; j<n; ++j) {
				sum += spectrum(i, theta0 + (theta1 - theta0)*j/n);
			}
		}
		return sum;
	}

	float variance() const { return _variance; }
	const Date& date() const { return _date; }

private:
	Map& _data;
	Date _date;
	const std::vector<float>& _frequencies;
	float _variance;

	static constexpr float PI = std::acos(-1.0f);
};

struct Station_kernel: public Kernel {

	typedef std::unordered_map<Variable, Observation> Map;
	typedef stdx::log<Station_kernel> this_log;

	Station_kernel(const Map& m, Station st):
		_observations(m), _station(st), _count(0) {}

	int check_read(const std::string filename, int ret) {
		if (ret == -1) {
			std::stringstream msg;
			msg << "Error while reading file '" << filename << "'.";
			throw components::Error(msg.str(), __FILE__, __LINE__, __func__);
		}
		return ret;
	}

	void write_output_to(std::ostream& out, Year year) {
		std::for_each(_out_matrix.cbegin(), _out_matrix.cend(),
			[this, &out, year] (const decltype(_out_matrix)::value_type& pair) {
				out << year << ',' << _station << ',' << pair.second << '\n';
			}
		);
		out << std::flush;
	}

	void react(Kernel* kernel) override {
		Spectrum_kernel* k = reinterpret_cast<Spectrum_kernel*>(kernel);
		_out_matrix[k->date()] = k->variance();
//		this_log() << "Finished station = " << _station
//			<< ", date = " << k->date()
//			<< ", variance = " << k->variance() << std::endl;
		if (++_count == _matrix.size()) {
			commit(local_server());
		}
	}

	void act() override {
		// omit missing variables
		if (_observations.size() != NUM_VARIABLES) {
			commit(local_server());
			return;
		}
		std::for_each(_observations.cbegin(), _observations.cend(),
			[this] (const decltype(_observations)::value_type& pair) {
				const Observation& ob = pair.second;
//				this_log() << _station << ' ' << pair.first << ' ' << pair.second << std::endl;
				this_log() << ob.filename() << std::endl;
				::gzFile file = ::gzopen(ob.filename().c_str(), "rb");
				if (file == NULL) {
					std::stringstream msg;
					msg << "Can not open file '" << ob.filename() << "' for reading.";
					throw components::Error(msg.str(), __FILE__, __LINE__, __func__);
				}
				char buf[64];
				int count = 0;
				std::stringstream contents;
				while (check_read(ob.filename(), count=::gzread(file, buf, sizeof(buf))) != 0) {
					contents.write(buf, count);
				}
				// skip header
				if (_frequencies.size() == 0) {
					contents.ignore(16);
					char ch;
					while (contents && (ch = contents.get()) != '\n') {
						contents.putback(ch);
						float value;
						contents >> value;
						_frequencies.push_back(value);
					}
				} else {
					contents.ignore(1000, '\n');
				}

				Date date;
				while (contents >> date) {
					char ch;
					while (contents && (ch = contents.get()) != '\n') {
						contents.putback(ch);
						float value;
						contents >> value;
						_matrix[date][ob.variable()].push_back(value);
					}
				}
				::gzclose(file);
			}
		);
		// omit missing records for given date
		this_log() << "Size = " << _matrix.size() << std::endl;
		for (auto it=_matrix.begin(); it!=_matrix.end(); ) {
			if (it->second.size() != NUM_VARIABLES) {
				it = _matrix.erase(it);
			} else {
				++it;
			}
		}
		this_log() << "New size = " << _matrix.size() << std::endl;
		this_log log;
		log << "Frequencies: ";
		std::for_each(
			_frequencies.cbegin(), _frequencies.cend(),
			[&log] (float rhs) {
				log << rhs << ' ';
			}
		);
		log << std::endl;

		std::for_each(_matrix.begin(), _matrix.end(),
			[this] (decltype(_matrix)::value_type& pair) {
				Spectrum_kernel* k = new Spectrum_kernel(pair.second, pair.first, _frequencies);
//				k->setf(Kernel::Flag::priority_service);
				this->upstream(local_server(), k);
			}
		);
	}

	Station station() const { return _station; }
	int32_t num_processed_spectra() const { return _matrix.size(); }

private:
	Map _observations;
	Station _station;
	std::map<Date, std::unordered_map<Variable, std::vector<float>>> _matrix;
	std::vector<float> _frequencies;
	uint32_t _count;
	std::map<Date, float> _out_matrix;

	static const int NUM_VARIABLES = 5;
};

struct Year_kernel: public Kernel {

	typedef std::unordered_map<Station,
		std::unordered_map<Variable, Observation>> Map;
	typedef stdx::log<Year_kernel> this_log;

	Year_kernel():
		_count(0), _num_spectra(0),
		_output_file()
	{}

	Year_kernel(const Map& m, Year year):
		_observations(m), _year(year), _count(0), _num_spectra(0),
		_output_file()
	{}

	Resource resource() const {
		return std::to_string(_year);
	}

	std::string output_filename() const {
		return std::to_string(_year) + ".out";
	}

	void react(Kernel* kernel) override {
		Station_kernel* k = reinterpret_cast<Station_kernel*>(kernel);
		if (!_output_file.is_open()) {
			_output_file.open(output_filename());
		}
		k->write_output_to(_output_file, _year);
		this_log() << "Finished station " << k->station()
			<< " [" << 1+_count << '/' << _observations.size() << "] ("
			<< k->num_processed_spectra() << " spectra total)" << std::endl;
		_num_spectra += k->num_processed_spectra();
		if (++_count == _observations.size()) {
			commit(remote_server());
		}
	}

	void act() override {
		std::for_each(_observations.cbegin(), _observations.cend(),
			[this] (const decltype(_observations)::value_type& pair) {
				this->upstream(local_server(), new Station_kernel(pair.second, pair.first));
//				std::for_each(pair.second.cbegin(), pair.second.cend(),
//					[this] (const decltype(pair.second)::value_type& pair2) {
//						this_log() << _year << ' ' << pair2.first << ' ' << pair2.second << std::endl;
//					}
//				);
			}
		);
	}

	void
	read(sys::packetstream& in) override {
		in >> _year;
		int32_t num_stations;
		in >> num_stations;
		for (int32_t i=0; i<num_stations; ++i) {
			int32_t num_observations;
			in >> num_observations;
			for (int32_t i=0; i<num_observations; ++i) {
				Observation ob;
				in >> ob;
				_observations[ob.station()][ob.variable()] = ob;
			}
		}
		in >> _num_spectra;
	}

	void
	write(sys::packetstream& out) override {
		out << _year;
		out << int32_t(_observations.size());
		std::for_each(_observations.cbegin(), _observations.cend(),
			[&out] (const decltype(_observations)::value_type& pair) {
				out << int32_t(pair.second.size());
				std::for_each(pair.second.cbegin(), pair.second.cend(),
					[&out] (const decltype(pair.second)::value_type& pair2) {
						out << pair2.second;
					}
				);
			}
		);
		out << _num_spectra;
	}

	const Type<Kernel>
	type() const noexcept override {
		return static_type();
	}

	static const Type<Kernel>
	static_type() noexcept {
		return Type<Kernel>{
			10000,
			"Year_kernel",
			[] (sys::packetstream& in) {
				Year_kernel* k = new Year_kernel;
				k->read(in);
				return k;
			}
		};
	}

	Year year() const { return _year; }
	int32_t num_processed_spectra() const { return _num_spectra; }

private:
	Map _observations;
	Year _year;
	uint32_t _count;
	int32_t _num_spectra;
	std::ofstream _output_file;
};

struct Launcher: public Kernel {

	typedef stdx::log<Launcher> this_log;

	Launcher(Server& this_server, int argc, char** argv): _count(0), _count_spectra(0) {}
	void act() {
		std::ifstream in("input");
		std::unordered_map<Year,
			std::unordered_map<Station,
				std::unordered_map<Variable, Observation>>>
					observations;
		Observation ob;
		while (in >> ob) {
			in.ignore(100, '\n');
			observations[ob.year()][ob.station()][ob.variable()] = ob;
			this_log() << "LINE " << ob << std::endl;
		}
		_count -= observations.size();
		std::for_each(observations.cbegin(), observations.cend(),
			[this] (const decltype(observations)::value_type& pair) {
				this->upstream(remote_server(), new Year_kernel(pair.second, pair.first));
			}
		);
	}
	void react(Kernel* kernel) {
		Year_kernel* k = reinterpret_cast<Year_kernel*>(kernel);
		this_log() << "Finished year " << k->year() << std::endl;
		_count_spectra += k->num_processed_spectra();
		if (++_count == 0) {
			this_log() << "Total number of processed spectra: " << _count_spectra << std::endl;
			commit(local_server());
		}
	}

private:
	int32_t _count;
	int32_t _count_spectra;
};

int
main(int argc, char** argv) {
	return factory_main<Launcher,config>(argc, argv);
}
