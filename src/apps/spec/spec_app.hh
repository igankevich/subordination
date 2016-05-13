#ifndef APPS_SPEC_SPEC_APP_HH
#define APPS_SPEC_SPEC_APP_HH

#include <zlib.h>

#include <fstream>

#include <sys/path.hh>

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

	friend std::istream&
	operator>>(std::istream& in, Date& rhs) {
		return in >> rhs._year >> rhs._month >> rhs._day >> rhs._hours >> rhs._minutes;
	}

	friend std::ostream&
	operator<<(std::ostream& out, const Date& rhs) {
		return out << std::setfill('0')
			                << rhs._year << ' '
			<< std::setw(2) << rhs._month << ' '
			<< std::setw(2) << rhs._day << ' '
			<< std::setw(2) << rhs._hours << ' '
			<< std::setw(2) << rhs._minutes;
	}

	friend sys::packetstream&
	operator>>(sys::packetstream& in, Date& rhs) {
		return in >> rhs._year >> rhs._month >> rhs._day >> rhs._hours >> rhs._minutes;
	}

	friend sys::packetstream&
	operator<<(sys::packetstream& out, const Date& rhs) {
		return out << rhs._year << rhs._month << rhs._day << rhs._hours << rhs._minutes;
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

namespace sys {

	namespace bits {

		template<class Container>
		sys::packetstream&
		write_container(sys::packetstream& out, const Container& rhs) {
			typedef typename Container::value_type value_type;
			out << uint64_t(rhs.size());
			std::for_each(
				rhs.begin(),
				rhs.end(),
				[&out] (const value_type& val) {
					out << val;
				}
			);
			return out;
		}

		template<class Container>
		sys::packetstream&
		read_container(sys::packetstream& in, Container& rhs) {
			typedef typename Container::value_type value_type;
			uint64_t n = 0;
			in >> n;
			rhs.resize(n);
			std::for_each(
				rhs.begin(),
				rhs.end(),
				[&in] (value_type& val) {
					in >> val;
				}
			);
			return in;
		}

		template<class Container>
		sys::packetstream&
		read_map(sys::packetstream& in, Container& rhs) {
			typedef typename Container::key_type key_type;
			typedef typename Container::mapped_type mapped_type;
			uint64_t n = 0;
			in >> n;
			rhs.clear();
			for (uint64_t i=0; i<n; ++i) {
				key_type key;
				mapped_type val;
				in >> key >> val;
				rhs.emplace(key, val);
			}
			return in;
		}

	}

	template<class T>
	sys::packetstream&
	operator<<(sys::packetstream& out, const std::vector<T>& rhs) {
		return bits::write_container(out, rhs);
	}

	template<class T>
	sys::packetstream&
	operator>>(sys::packetstream& in, std::vector<T>& rhs) {
		return bits::read_container(in, rhs);
	}

	template<class K, class V>
	sys::packetstream&
	operator<<(sys::packetstream& out, const std::unordered_map<K,V>& rhs) {
		return bits::write_container(out, rhs);
	}

	template<class K, class V>
	sys::packetstream&
	operator>>(sys::packetstream& in, std::unordered_map<K,V>& rhs) {
		return bits::read_map(in, rhs);
	}

	template<class K, class V>
	sys::packetstream&
	operator<<(sys::packetstream& out, const std::map<K,V>& rhs) {
		return bits::write_container(out, rhs);
	}

	template<class K, class V>
	sys::packetstream&
	operator>>(sys::packetstream& in, std::map<K,V>& rhs) {
		return bits::read_map(in, rhs);
	}

	template<class X, class Y>
	sys::packetstream&
	operator<<(sys::packetstream& out, const std::pair<X,Y>& rhs) {
		return out << rhs.first << rhs.second;
	}

	template<class X, class Y>
	sys::packetstream&
	operator>>(sys::packetstream& in, std::pair<X,Y>& rhs) {
		return in >> rhs.first >> rhs.second;
	}


}

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
		commit(local_server, this);
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

	static const float PI;
};

const float Spectrum_kernel::PI = std::acos(-1.0f);

struct Station_kernel: public Kernel {

	typedef std::unordered_map<Variable, Observation> Map;

	Station_kernel() = default;

	Station_kernel(const Map& m, Station st, Year year):
	_observations(m), _station(st), _year(year), _count(0)
	{}

	int check_read(const std::string filename, int ret) {
		if (ret == -1) {
			std::stringstream msg;
			msg << "Error while reading file '" << filename << "'.";
			throw Error(msg.str(), __FILE__, __LINE__, __func__);
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
		Spectrum_kernel* k = dynamic_cast<Spectrum_kernel*>(kernel);
		_out_matrix[k->date()] = k->variance();
		if (--_count == 0) {
			#ifndef NDEBUG
			stdx::debug_message(
				"spec",
				"finished station _, year _, total no. of spectra _",
				station(), _year, num_processed_spectra()
			);
			#endif
			_observations.clear();
			_spectra.clear();
			commit(remote_server, this);
		}
	}

	void act() override {
		// skip records when some variables are missing
		if (_observations.size() != NUM_VARIABLES) {
			commit(remote_server, this);
			return;
		}
		std::for_each(_observations.cbegin(), _observations.cend(),
			[this] (const decltype(_observations)::value_type& pair) {
				const Observation& ob = pair.second;
				::gzFile file = ::gzopen(ob.filename().c_str(), "rb");
				if (file == NULL) {
					std::stringstream msg;
					msg << "Can not open file '" << ob.filename() << "' for reading.";
					throw Error(msg.str(), __FILE__, __LINE__, __func__);
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
						_spectra[date][ob.variable()].push_back(value);
					}
				}
				::gzclose(file);
			}
		);
		// remove incomplete records for given date
		const size_t old_size = _spectra.size();
		for (auto it=_spectra.begin(); it!=_spectra.end(); ) {
			if (it->second.size() != NUM_VARIABLES) {
				it = _spectra.erase(it);
			} else {
				++it;
			}
		}
		const size_t new_size = _spectra.size();
		if (new_size < old_size) {
			#ifndef NDEBUG
			stdx::debug_message(
				"spec",
				"removed _ incomplete records from station _, year _",
				old_size-new_size, _station, _year
			);
			#endif
		}

		_count = _spectra.size();
		std::for_each(_spectra.begin(), _spectra.end(),
			[this] (decltype(_spectra)::value_type& pair) {
				Spectrum_kernel* k = new Spectrum_kernel(pair.second, pair.first, _frequencies);
//				k->setf(Kernel::Flag::priority_service);
				upstream(local_server, this, k);
			}
		);
	}

	Year year() const { return _year; }
	Station station() const { return _station; }
	int32_t num_processed_spectra() const { return _out_matrix.size(); }

	void
	read(sys::packetstream& in) override {
		Kernel::read(in);
		in >> _observations;
		in >> _station;
		in >> _year;
		in >> _frequencies;
		in >> _count;
		in >> _out_matrix;
	}

	void
	write(sys::packetstream& out) override {
		Kernel::write(out);
		out << _observations;
		out << _station;
		out << _year;
		out << _frequencies;
		out << _count;
		out << _out_matrix;
	}

private:
	Map _observations;
	Station _station;
	Year _year;
	std::vector<float> _frequencies;
	uint32_t _count = 0;
	std::map<Date, std::unordered_map<Variable, std::vector<float>>> _spectra;
	std::map<Date, float> _out_matrix;

	static const int NUM_VARIABLES = 5;
};

struct Year_kernel: public Kernel {

	typedef std::unordered_map<Station,
		std::unordered_map<Variable, Observation>> Map;

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

	bool
	finished() const noexcept {
		return _count == _observations.size();
	}

	void react(Kernel* kernel) override {
		Station_kernel* k = dynamic_cast<Station_kernel*>(kernel);
		if (!_output_file.is_open()) {
			_output_file.open(output_filename());
		}
		k->write_output_to(_output_file, _year);
		#ifndef NDEBUG
		stdx::debug_message(
			"spec",
			"finished station _, year _, [_/_], total no. of spectra _, from _",
			k->station(), _year, 1+_count, _observations.size(),
			k->num_processed_spectra(), k->from()
		);
		#endif
		_num_spectra += k->num_processed_spectra();
		if (++_count == _observations.size()) {
//			commit(remote_server());
		}
	}

	void act() override {
		std::for_each(_observations.cbegin(), _observations.cend(),
			[this] (const decltype(_observations)::value_type& pair) {
				upstream(remote_server, this, new Station_kernel(pair.second, pair.first, _year));
			}
		);
	}

	void
	read(sys::packetstream& in) override {
		Kernel::read(in);
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
		Kernel::write(out);
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

	typedef std::unordered_map<Station,
		std::unordered_map<Variable, Observation>> Map;

	typedef std::chrono::nanoseconds::rep Time;

	Launcher(): _count(0), _count_spectra(0) {}

	static Time
	current_time_nano() {
		using namespace std::chrono;
		typedef std::chrono::system_clock Clock;
		return duration_cast<nanoseconds>(Clock::now().time_since_epoch()).count();
	}

	void act() override {
		#ifndef NDEBUG
		stdx::debug_message("spec", "launcher start");
		#endif
		_time0 = current_time_nano();
		std::ifstream in("input");
		std::unordered_map<Year,
			std::unordered_map<Station,
				std::unordered_map<Variable, Observation>>>
					observations;
		Observation ob;
		while (in >> ob) {
			in.ignore(100, '\n');
			observations[ob.year()][ob.station()][ob.variable()] = ob;
		}
		_count -= observations.size();
		std::for_each(observations.cbegin(), observations.cend(),
			[this] (const decltype(observations)::value_type& pair) {
				Year year = pair.first;
				_yearkernels[year] = new Year_kernel(pair.second, year);
				submit_station_kernels(pair.second, year);
				// this->upstream(remote_server(), new Year_kernel(pair.second, year));
			}
		);
	}

	void react(Kernel* kernel) override {
		// Year_kernel* k = dynamic_cast<Year_kernel*>(kernel);
		Station_kernel* k1 = dynamic_cast<Station_kernel*>(kernel);
		Year_kernel* k = _yearkernels[k1->year()];
		k->react(k1);
		if (k->finished()) {
			#ifndef NDEBUG
			stdx::debug_message("spec", "finished year _", k->year());
			#endif
			_count_spectra += k->num_processed_spectra();
			_yearkernels.erase(k1->year());
			delete k;
			if (++_count == 0) {
				#ifndef NDEBUG
				stdx::debug_message("spec", "total number of processed spectra _", _count_spectra);
				#endif
				{
					Time time1 = current_time_nano();
					std::ofstream timerun_log("time.log");
					timerun_log << float(time1 - _time0)/1000/1000/1000 << std::endl;
				}
				{
					std::ofstream log("nspectra.log");
					log << _count_spectra << std::endl;
				}
				#if defined(FACTORY_TEST_SLAVE_FAILURE)
				commit(local_server, this);
				#else
				commit(remote_server, this);
				#endif
			}
		}
	}

	void
	read(sys::packetstream& in) override {
		Kernel::read(in);
		in >> _count >> _count_spectra;
	}

	void
	write(sys::packetstream& out) override {
		Kernel::write(out);
		out << _count << _count_spectra;
	}

	void
	submit_station_kernels(Map _observations, Year _year) {
		std::for_each(_observations.cbegin(), _observations.cend(),
			[this,&_observations,_year] (const decltype(_observations)::value_type& pair) {
				upstream(remote_server, this, new Station_kernel(pair.second, pair.first, _year));
			}
		);
	}

private:
	int32_t _count;
	int32_t _count_spectra;
	Time _time0;

	std::unordered_map<Year, Year_kernel*> _yearkernels;
};

struct Spec_app: public Kernel {

	void
	act() override {
		#ifndef NDEBUG
		stdx::debug_message("spec", "program start");
		#endif
		#if defined(FACTORY_TEST_SLAVE_FAILURE)
		upstream(local_server, this, new Launcher);
		#else
		upstream_carry(remote_server, this, new Launcher);
		#endif
	}

	void
	react(Kernel*) override {
		commit(local_server, this);
	}

};


#endif // APPS_SPEC_SPEC_APP_HH
