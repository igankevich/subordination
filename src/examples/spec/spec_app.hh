#ifndef EXAMPLES_SPEC_SPEC_APP_HH
#define EXAMPLES_SPEC_SPEC_APP_HH

#include <zlib.h>

#include <array>
#include <bitset>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
#include <sstream>

#include <unistdx/base/log_message>
#include <unistdx/fs/idirtree>
#include <unistdx/fs/path>

#include <subordination/api.hh>
#include <subordination/core/error.hh>

using Year = int32_t;
using Month = int32_t;
using Day = int32_t;
using Hour = int32_t;
using Minute = int32_t;
using Station = int32_t;

constexpr const auto max_variables = 5;

enum class Variable: int {D=0, I=1, J=2, K=3, W=4};

Variable string_to_variable(const std::string& s) {
    if (s.empty()) { throw std::invalid_argument("bad variable"); }
    switch (s.front()) {
        case 'd': return Variable::D;
        case 'i': return Variable::I;
        case 'j': return Variable::J;
        case 'k': return Variable::K;
        case 'w': return Variable::W;
        default: throw std::invalid_argument("bad variable");
    }
}

namespace std {
    template <>
    class hash<Variable>: public hash<std::underlying_type<Variable>::type> {
    private:
        using tp = std::underlying_type<Variable>::type;
    public:
        size_t operator()(Variable v) const noexcept {
            return this->hash<tp>::operator()(tp(v));
        }
    };
}

class Timestamp {

private:
    std::time_t _timestamp{};

public:

    Timestamp() = default;
    ~Timestamp() = default;
    Timestamp(const Timestamp&) = default;
    Timestamp& operator=(const Timestamp&) = default;
    Timestamp(Timestamp&&) = default;
    Timestamp& operator=(Timestamp&&) = default;

    inline std::time_t get() const noexcept { return this->_timestamp; }

    inline bool operator==(const Timestamp& rhs) const noexcept {
        return this->_timestamp == rhs._timestamp;
    }

    inline bool operator<(const Timestamp& rhs) const noexcept {
        return this->_timestamp < rhs._timestamp;
    }

    friend std::istream&
    operator>>(std::istream& in, Timestamp& rhs) {
        // YYYY MM DD hh mm
        // 2010 01 01 00 00
        std::tm t{};
        in >> std::get_time(&t, "%Y %m %d %H %M");
        rhs._timestamp = std::mktime(&t);
        return in;
    }

    friend std::ostream&
    operator<<(std::ostream& out, const Timestamp& rhs) {
        std::tm* t = std::gmtime(&rhs._timestamp);
        out << std::put_time(t, "%Y %m %d %H %M");
        return out;
    }

    friend sbn::kernel_buffer&
    operator>>(sbn::kernel_buffer& in, Timestamp& rhs) {
        return in >> rhs._timestamp;
    }

    friend sbn::kernel_buffer&
    operator<<(sbn::kernel_buffer& out, const Timestamp& rhs) {
        return out << rhs._timestamp;
    }

};

namespace std {
    template <>
    class hash<Timestamp>: public hash<std::time_t> {
    public:
        size_t operator()(Timestamp t) const noexcept {
            return this->hash<std::time_t>::operator()(t.get());
        }
    };
}

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

class Spectrum_file {

private:
    sys::path _filename;
    Station _station = 0;
    Year _year = 0;
    Variable _variable{};

public:

    Spectrum_file() = default;
    ~Spectrum_file() = default;
    Spectrum_file(const Spectrum_file&) = default;
    Spectrum_file& operator=(const Spectrum_file&) = default;
    Spectrum_file(Spectrum_file&&) = default;
    Spectrum_file& operator=(Spectrum_file&&) = default;

    inline Spectrum_file(const sys::path& filename, Station station, Variable var, Year year):
    _filename(filename), _station(station), _year(year), _variable(var) {}

    inline const sys::path& filename() const noexcept { return this->_filename; }
    inline Station station() const noexcept { return this->_station; }
    inline Variable variable() const noexcept { return this->_variable; }
    inline Year year() const noexcept { return this->_year; }

    friend sbn::kernel_buffer& operator>>(sbn::kernel_buffer& in, Spectrum_file& rhs) {
        return in >> rhs._filename >> rhs._year >> rhs._station >> rhs._variable;
    }

    friend sbn::kernel_buffer& operator<<(sbn::kernel_buffer& out, const Spectrum_file& rhs) {
        return out << rhs._filename << rhs._year << rhs._station << rhs._variable;
    }

};

namespace sys {

    namespace bits {

        template<class Container>
        sbn::kernel_buffer&
        write_container(sbn::kernel_buffer& out, const Container& rhs) {
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
        sbn::kernel_buffer&
        read_container(sbn::kernel_buffer& in, Container& rhs) {
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
        sbn::kernel_buffer&
        read_map(sbn::kernel_buffer& in, Container& rhs) {
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
    sbn::kernel_buffer&
    operator<<(sbn::kernel_buffer& out, const std::vector<T>& rhs) {
        return bits::write_container(out, rhs);
    }

    template<class T>
    sbn::kernel_buffer&
    operator>>(sbn::kernel_buffer& in, std::vector<T>& rhs) {
        return bits::read_container(in, rhs);
    }

    template<class K, class V>
    sbn::kernel_buffer&
    operator<<(sbn::kernel_buffer& out, const std::unordered_map<K,V>& rhs) {
        return bits::write_container(out, rhs);
    }

    template<class K, class V>
    sbn::kernel_buffer&
    operator>>(sbn::kernel_buffer& in, std::unordered_map<K,V>& rhs) {
        return bits::read_map(in, rhs);
    }

    template<class K, class V>
    sbn::kernel_buffer&
    operator<<(sbn::kernel_buffer& out, const std::map<K,V>& rhs) {
        return bits::write_container(out, rhs);
    }

    template<class K, class V>
    sbn::kernel_buffer&
    operator>>(sbn::kernel_buffer& in, std::map<K,V>& rhs) {
        return bits::read_map(in, rhs);
    }

    template<class X, class Y>
    sbn::kernel_buffer&
    operator<<(sbn::kernel_buffer& out, const std::pair<X,Y>& rhs) {
        return out << rhs.first << rhs.second;
    }

    template<class X, class Y>
    sbn::kernel_buffer&
    operator>>(sbn::kernel_buffer& in, std::pair<X,Y>& rhs) {
        return in >> rhs.first >> rhs.second;
    }


}

static constexpr const int DENSITY = int(Variable::W);
static constexpr const int ALPHA_1 = int(Variable::D);
static constexpr const int ALPHA_2 = int(Variable::I);
static constexpr const int R_1     = int(Variable::J);
static constexpr const int R_2     = int(Variable::K);

template <class T>
class Spectrum_kernel: public sbn::kernel {

public:

    using Map = std::array<std::vector<T>,max_variables>;

private:
    Map& _data;
    Timestamp _date{};
    const std::vector<T>& _frequencies;
    T _variance{};

public:

    Spectrum_kernel(Map& m, Timestamp d, const std::vector<T>& freq):
        _data(m), _date(d), _frequencies(freq), _variance(0)
    {}

    void act() override {
        _variance = compute_variance();
        sbn::commit(std::move(this_ptr()));
    }

    T spectrum(int32_t i, T angle) {
        auto density = this->_data[DENSITY][i];
        auto r1 = this->_data[R_1][i];
        auto r2 = this->_data[R_2][i];
        auto alpha1 = this->_data[ALPHA_1][i];
        auto alpha2 = this->_data[ALPHA_2][i];
        return density * (T{1}/T{M_PI}) * (T{0.5}
            + T{0.01}*r1*std::cos(      angle - alpha1)
            + T{0.01}*r2*std::cos(T{2}*(angle - alpha2)));
    }

    T compute_variance() {
        const T theta0 = 0;
        const T theta1 = 2.0f*M_PI;
        int32_t n = _frequencies.size();
        T sum = 0;
        for (int32_t i=0; i<n; ++i) {
            for (int32_t j=0; j<n; ++j) {
                sum += spectrum(i, theta0 + (theta1 - theta0)*j/n);
            }
        }
        return sum;
    }

    inline T variance() const noexcept { return this->_variance; }
    inline Timestamp date() const noexcept { return this->_date; }

};

class GZIP_file {

public:
    using file_type = ::gzFile;

private:
    file_type _file{};

public:

    GZIP_file() = default;
    ~GZIP_file() = default;
    GZIP_file(const GZIP_file&) = delete;
    GZIP_file& operator=(const GZIP_file&) = delete;

    inline GZIP_file(GZIP_file&& rhs): _file(rhs._file) { rhs._file = nullptr; }

    inline GZIP_file& operator=(GZIP_file&& rhs) {
        swap(rhs);
        return *this;
    }

    inline void swap(GZIP_file& rhs) {
        std::swap(this->_file, rhs._file);
    }

    inline void open(const char* filename, const char* flags) {
        this->_file = ::gzopen(filename, flags);
        if (!this->_file) {
            sbn::throw_error("unable to open ", filename);
        }
    }

    inline void close() {
        ::gzclose(this->_file);
    }

    inline int read(void* ptr, unsigned len) {
        int ret = ::gzread(this->_file, ptr, len);
        if (ret == -1) { sbn::throw_error("i/o error"); }
        return ret;
    }

};

inline void swap(GZIP_file& a, GZIP_file& b) { a.swap(b); }

template <class T>
class Spectrum_file_kernel: public sbn::kernel {

public:
    using spectrum_file_array = std::vector<Spectrum_file>;

private:
    spectrum_file_array _files;
    std::vector<T> _frequencies;
    uint32_t _count = 0;
    std::map<Timestamp, std::array<std::vector<T>,max_variables>> _spectra;
    std::map<Timestamp, T> _out_matrix;

public:

    Spectrum_file_kernel() = default;

    inline explicit Spectrum_file_kernel(const spectrum_file_array& files): _files(files) {}

    void act() override {
        char buf[4096];
        for (const auto& f : this->_files) {
            GZIP_file in;
            in.open(f.filename().data(), "rb");
            int count = 0;
            std::stringstream contents;
            while ((count=in.read(buf, sizeof(buf))) != 0) {
                contents.write(buf, count);
            }
            std::string line;
            std::stringstream str;
            while (!(contents >> std::ws).eof()) {
                std::getline(contents, line, '\n');
                if (line.empty()) { continue; }
                if (line.front() == '#') {
                    // read frequencies
                    if (_frequencies.size() == 0) {
                        str.clear();
                        str.str(line);
                        str.ignore(16);
                        T value;
                        while (str >> value) {
                            this->_frequencies.emplace_back(std::move(value));
                        }
                    } else {
                        // skip lines starting with "#"
                    }
                } else {
                    Timestamp timestamp;
                    str.clear();
                    str.str(line);
                    str >> timestamp;
                    T value;
                    while (str >> value) {
                        if (std::abs(value-T{999}) < T{1e-1}) { value = 0; }
                        this->_spectra[timestamp][int(f.variable())].emplace_back(std::move(value));
                    }
                }
            }
            in.close();
        }
        // remove incomplete records for given date
        const size_t old_size = _spectra.size();
        for (auto it=_spectra.begin(); it!=_spectra.end(); ) {
            const auto& array = it->second;
            bool any_empty = std::any_of(
                array.begin(), array.end(),
                [] (const std::vector<T>& rhs) { return rhs.empty(); });
            if (any_empty) {
                it = _spectra.erase(it);
            } else {
                ++it;
            }
        }
        const size_t new_size = _spectra.size();
        if (new_size < old_size) {
            sys::log_message(
                "spec",
                "removed _ incomplete records from station _, year _",
                old_size-new_size, station(), year()
            );
        }
        _count = _spectra.size();
        for (auto& pair : this->_spectra) {
            auto k =
                sbn::make_pointer<Spectrum_kernel<T>>(pair.second, pair.first, _frequencies);
            sbn::upstream(this, std::move(k));
        }
    }

    void write_output_to(std::ostream& out) {
        std::for_each(_out_matrix.cbegin(), _out_matrix.cend(),
            [this, &out] (const typename decltype(_out_matrix)::value_type& pair) {
                out << year() << ',' << station() << ',' << pair.second << '\n';
            }
        );
        out << std::flush;
    }

    void react(sbn::kernel_ptr&& child) override {
        auto k = sbn::pointer_dynamic_cast<Spectrum_kernel<T>>(std::move(child));
        _out_matrix[k->date()] = k->variance();
        if (--_count == 0) {
            _files.clear();
            _spectra.clear();
            sbn::commit<sbn::Remote>(std::move(this_ptr()));
        }
    }

    inline Year year() const noexcept {
        if (this->_files.empty()) { return {}; }
        return this->_files.front().year();
    }

    inline Station station() const noexcept {
        if (this->_files.empty()) { return {}; }
        return this->_files.front().station();
    }

    inline int32_t num_processed_spectra() const noexcept { return _out_matrix.size(); }

    void
    read(sbn::kernel_buffer& in) override {
        kernel::read(in);
        in >> _files;
        in >> _frequencies;
        in >> _count;
        in >> _out_matrix;
    }

    void
    write(sbn::kernel_buffer& out) const override {
        kernel::write(out);
        out << _files;
        out << _frequencies;
        out << _count;
        out << _out_matrix;
    }

};

template <class T>
class Spectrum_directory_kernel: public sbn::kernel {

public:
    using clock_type = std::chrono::system_clock;
    using time_point = clock_type::time_point;
    using duration = clock_type::duration;
    using spectrum_file_array = std::vector<Spectrum_file>;

private:
    std::vector<sys::path> _input_directories;
    int32_t _count = 0;
    int32_t _count_spectra = 0;
    int32_t _num_kernels = 0;
    std::array<time_point,2> _time_points;
    std::unordered_map<Year,std::ofstream> _output_files;

public:
    Spectrum_directory_kernel() = default;

    inline explicit Spectrum_directory_kernel(const std::vector<sys::path>& input_directories):
    _input_directories(input_directories) {}

    void act() override {
        this->_time_points[0] = clock_type::now();
        sys::idirtree tree;
        std::regex rx("([0-9]+)([dijkw])([0-9]+)\\.txt\\.gz");
        std::cmatch match;
        std::map<std::pair<Year,Station>,std::vector<Spectrum_file>> files;
        for (const auto& path : this->_input_directories) {
            tree.open(path);
            while (!tree.eof()) {
                tree.clear();
                for (const auto& entry : tree) {
                    sys::path path(tree.current_dir(), entry.name());
                    if (std::regex_match(entry.name(), match, rx)) {
                        sys::log_message(
                            "spec", "file _ station _ variable _ year _",
                            path, match[1], match[2], match[3]);
                        Station station = std::stoi(match[1].str());
                        Year year = std::stoi(match[3].str());
                        files[std::make_pair(year,station)].emplace_back(
                            path, station, string_to_variable(match[2].str()),
                            year);
                    }
                }
            }
        }
        for (const auto& pair : files) {
            // skip incomplete records that have less than five files
            if (!complete(pair.second)) { continue; }
            ++this->_num_kernels;
            sbn::upstream<sbn::Remote>(this, sbn::make_pointer<Spectrum_file_kernel<T>>(pair.second));
        }
    }

    void react(sbn::kernel_ptr&& child) override {
        auto k = sbn::pointer_dynamic_cast<Spectrum_file_kernel<T>>(std::move(child));
        auto year = k->year();
        auto& output_file = this->_output_files[year];
        if (!output_file.is_open()) {
            output_file.open(std::to_string(year) + ".out");
        }
        k->write_output_to(output_file);
        this->_count_spectra += k->num_processed_spectra();
        sys::log_message(
            "spec",
            "[_/_] finished station _, year _, total no. of spectra _",
            this->_count+1, this->_num_kernels,
            k->station(), k->year(), k->num_processed_spectra()
        );
        if (++_count == this->_num_kernels) {
            for (auto& pair : this->_output_files) { pair.second.close(); }
            sys::log_message("spec", "total number of processed spectra _", _count_spectra);
            {
                using namespace std::chrono;
                this->_time_points[1] = clock_type::now();
                const auto& t = this->_time_points;
                std::ofstream out("time.log");
                out << duration_cast<microseconds>(t[1] - t[0]).count() << std::endl;
                out.close();
            }
            {
                std::ofstream log("nspectra.log");
                log << _count_spectra << std::endl;
            }
            #if defined(SUBORDINATION_TEST_SLAVE_FAILURE)
            sbn::commit<sbn::Local>(std::move(this_ptr()));
            #else
            sbn::commit<sbn::Remote>(std::move(this_ptr()));
            #endif
        }
    }

    void
    read(sbn::kernel_buffer& in) override {
        kernel::read(in);
        in >> _count >> _count_spectra;
    }

    void
    write(sbn::kernel_buffer& out) const override {
        kernel::write(out);
        out << _count << _count_spectra;
    }

private:

    bool complete(const std::vector<Spectrum_file>& files) {
        if (files.size() != 5) { return false; }
        std::bitset<5> variables;
        for (const auto& f : files) {
            variables.set(static_cast<int>(f.variable()));
        }
        return variables.all();
    }

};

template <class T>
class Main: public sbn::kernel {

private:
    std::vector<sys::path> _input_directories;

public:

    Main() = default;

    inline Main(int argc, char** argv) {
        for (int i=1; i<argc; ++i) {
            this->_input_directories.emplace_back(argv[i]);
        }
    }

    void act() override {
        #if defined(SBN_DEBUG)
        sys::log_message("spec", "program start");
        #endif
        #if defined(SUBORDINATION_TEST_SLAVE_FAILURE)
        sbn::upstream<sbn::Local>(
            this, sbn::make_pointer<Spectrum_directory_kernel<T>>(this->_input_directories));
        #else
        auto k =
            sbn::make_pointer<Spectrum_directory_kernel<T>>(this->_input_directories);
        k->setf(sbn::kernel_flag::carries_parent);
        sbn::upstream<sbn::Remote>(this, std::move(k));
        #endif
    }

    void react(sbn::kernel_ptr&&) override {
        sbn::commit<sbn::Local>(std::move(this_ptr()));
    }

};

#endif // vim:filetype=cpp
