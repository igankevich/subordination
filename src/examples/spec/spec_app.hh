#ifndef EXAMPLES_SPEC_SPEC_APP_HH
#define EXAMPLES_SPEC_SPEC_APP_HH

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

#include <spec/gzip_file.hh>
#include <spec/timestamp.hh>
#include <spec/variable.hh>

using Year = int32_t;
using Station = int32_t;

namespace  {

    template <class ... Args> inline static void
    log(const char* fmt, const Args& ... args) {
        sys::log_message("spec", fmt, args ...);
    }

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
    spec::Variable _variable{};

public:

    Spectrum_file() = default;
    ~Spectrum_file() = default;
    Spectrum_file(const Spectrum_file&) = default;
    Spectrum_file& operator=(const Spectrum_file&) = default;
    Spectrum_file(Spectrum_file&&) = default;
    Spectrum_file& operator=(Spectrum_file&&) = default;

    inline
    Spectrum_file(const sys::path& filename, Station station, spec::Variable var, Year year):
    _filename(filename), _station(station), _year(year), _variable(var) {}

    inline const sys::path& filename() const noexcept { return this->_filename; }
    inline Station station() const noexcept { return this->_station; }
    inline spec::Variable variable() const noexcept { return this->_variable; }
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

static constexpr const int DENSITY = int(spec::Variable::W);
static constexpr const int ALPHA_1 = int(spec::Variable::D);
static constexpr const int ALPHA_2 = int(spec::Variable::I);
static constexpr const int R_1     = int(spec::Variable::J);
static constexpr const int R_2     = int(spec::Variable::K);

template <class T>
class Variance_kernel: public sbn::kernel {

public:

    using Map = std::array<std::vector<T>,spec::max_variables>;

private:
    Map& _data;
    spec::Timestamp _date{};
    const std::vector<T>& _frequencies;
    T _variance{};

public:

    Variance_kernel(Map& m, spec::Timestamp d, const std::vector<T>& freq):
        _data(m), _date(d), _frequencies(freq), _variance(0)
    {}

    void act() override {
        //_variance = compute_variance();
        _variance = 0;
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
        int32_t n = std::min(this->_frequencies.size(), this->_data[0].size());
        for (const auto& array : this->_data) {
            int32_t new_n = array.size();
            if (new_n < n) { n = new_n; }
        }
        T sum = 0;
        for (int32_t i=0; i<n; ++i) {
            for (int32_t j=0; j<n; ++j) {
                sum += spectrum(i, theta0 + (theta1 - theta0)*j/n);
            }
        }
        return sum;
    }

    inline T variance() const noexcept { return this->_variance; }
    inline spec::Timestamp date() const noexcept { return this->_date; }

};


template <class T>
class File_kernel: public sbn::kernel {

private:
    Spectrum_file _file;
    std::vector<T> _frequencies;
    std::map<spec::Timestamp, std::vector<T>> _data;

public:

    File_kernel() = default;

    inline explicit
    File_kernel(const Spectrum_file& file): _file(file) {
        path(this->_file.filename());
    }

    void act() override {
        if (const char* hostname = std::getenv("SBN_TEST_SUBORDINATE_FAILURE")) {
            if (sys::this_process::hostname() == hostname) {
                sys::log_message("spec", "simulate subordinate failure _!", hostname);
                send(sys::signal::kill, sys::this_process::parent_id());
                send(sys::signal::kill, sys::this_process::id());
            }
        }
        char buf[4096];
        spec::GZIP_file in;
        // read directly from GlusterFS
        try {
            //sys::path new_path("/var", this->_file.filename());
            const auto& new_path = this->_file.filename();
            log("use new path _", new_path);
            in.open(new_path.data(), "rb");
        } catch (const std::exception& err) {
            log("use old path _", this->_file.filename());
            in.open(this->_file.filename().data(), "rb");
        }
        int count = 0;
        std::stringstream contents;
        while ((count=in.read(buf, sizeof(buf))) != 0) {
            contents.write(buf, count);
        }
        std::string line;
        std::stringstream str;
        int num_lines = 0;
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
                spec::Timestamp timestamp;
                str.clear();
                str.str(line);
                if (str >> timestamp) {
                    auto& spec = this->_data[timestamp];
                    T value;
                    while (str >> value) {
                        if (std::abs(value-T{999}) < T{1e-1}) { value = 0; }
                        spec.emplace_back(value);
                    }
                }
                ++num_lines;
            }
        }
        in.close();
        sys::log_message("spec", "_: _ records _ lines", this->_file.filename(),
                         this->_data.size(), num_lines);
        sbn::commit<sbn::Remote>(std::move(this_ptr()));
    }

    void write(sbn::kernel_buffer& out) const override {
        sbn::kernel::write(out);
        out << this->_file;
        out << this->_frequencies;
        out << this->_data;
    }

    void read(sbn::kernel_buffer& in) override {
        sbn::kernel::read(in);
        in >> this->_file;
        in >> this->_frequencies;
        in >> this->_data;
    }

    inline std::map<spec::Timestamp,std::vector<T>>& data() noexcept { return this->_data; }
    inline const Spectrum_file& file() const noexcept { return this->_file; }

};

template <class T>
class Five_files_kernel: public sbn::kernel {

public:
    using spectrum_file_array = std::vector<Spectrum_file>;
    enum class State { Reading, Processing };

private:
    spectrum_file_array _files;
    std::vector<T> _frequencies;
    uint32_t _count = 0;
    std::map<spec::Timestamp, std::array<std::vector<T>,spec::max_variables>> _spectra;
    std::map<spec::Timestamp, T> _out_matrix;
    State _state = State::Reading;

public:

    Five_files_kernel() = default;

    inline explicit
    Five_files_kernel(const spectrum_file_array& files): _files(files) {}

    void act() override {
        for (const auto& file : this->_files) {
            sbn::upstream<sbn::Remote>(this, sbn::make_pointer<File_kernel<T>>(file));
        }
    }

    void process_spectra() {
        #if defined(SBN_DEBUG)
        sys::log_message(
            "spec", "station _ year _ num-records _ num-frequencies _",
            station(), year(), this->_spectra.size(), this->_frequencies.size());
        #endif
        this->_state = State::Processing;
        // remove incomplete records for given date
        const size_t old_size = _spectra.size();
        remove_incomplete_records();
        const size_t new_size = _spectra.size();
        if (new_size < old_size) {
            sys::log_message(
                "spec",
                "removed _ incomplete records from station _, year _",
                old_size-new_size, station(), year());
        }
        this->_count = this->_spectra.size();
        if (this->_spectra.empty()) {
            sbn::commit<sbn::Remote>(std::move(this_ptr()));
        } else {
            for (auto& pair : this->_spectra) {
                auto k = sbn::make_pointer<Variance_kernel<T>>(
                    pair.second, pair.first, _frequencies);
                sbn::upstream(this, std::move(k));
            }
        }
    }

    void add_spectrum_variable(pointer<File_kernel<T>>&& k) {
        const auto variable = k->file().variable();
        for (auto& pair : k->data()) {
            this->_spectra[pair.first][int(variable)] = std::move(pair.second);
        }
        if (++this->_count == 5) { process_spectra(); }
    }

    void add_variance(pointer<Variance_kernel<T>>&& k) {
        this->_out_matrix[k->date()] = k->variance();
        if (--_count == 0) {
            sys::log_message("spec", "finished year _ station _", year(), station());
            if (!this->_files.empty()) { this->_files.resize(1); }
            this->_spectra.clear();
            sbn::commit<sbn::Remote>(std::move(this_ptr()));
        }
    }

    void react(sbn::kernel_ptr&& k) override {
        switch (this->_state) {
            case State::Reading:
                add_spectrum_variable(sbn::pointer_dynamic_cast<File_kernel<T>>(std::move(k)));
                break;
            case State::Processing:
                add_variance(sbn::pointer_dynamic_cast<Variance_kernel<T>>(std::move(k)));
                break;
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

    inline Year year() const noexcept {
        if (this->_files.empty()) { return {}; }
        return this->_files.front().year();
    }

    inline Station station() const noexcept {
        if (this->_files.empty()) { return {}; }
        return this->_files.front().station();
    }

    inline int32_t num_processed_spectra() const noexcept { return _out_matrix.size(); }

    void read(sbn::kernel_buffer& in) override {
        kernel::read(in);
        in >> _files;
        in >> _frequencies;
        in >> _count;
        in >> _out_matrix;
        in >> this->_state;
    }

    void write(sbn::kernel_buffer& out) const override {
        kernel::write(out);
        out << _files;
        out << _frequencies;
        out << _count;
        out << _out_matrix;
        out << this->_state;
    }

private:

    void remove_incomplete_records() {
        for (auto it=_spectra.begin(); it!=_spectra.end(); ) {
            const auto& array = it->second;
            auto front_size = array.front().size();
            bool any_empty = std::any_of(
                array.begin()+1, array.end(),
                [front_size] (const std::vector<T>& rhs) { return front_size != rhs.size(); });
            if (any_empty) {
                it = _spectra.erase(it);
            } else {
                ++it;
            }
        }
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
        if (const char* hostname = std::getenv("SBN_TEST_SUPERIOR_COPY_FAILURE")) {
            if (sys::this_process::hostname() == hostname) {
                if (const char* str = std::getenv("SBN_TEST_SLEEP_FOR")) {
                    auto seconds = std::atoi(str);
                    sys::log_message("spec", "sleeping for _ seconds", seconds);
                    std::this_thread::sleep_for(std::chrono::seconds(seconds));
                }
                sys::log_message("spec", "simulate superior copy failure _!", hostname);
                send(sys::signal::kill, sys::this_process::parent_id());
                send(sys::signal::kill, sys::this_process::id());
            }
        }
        sys::log_message("spec", "spectrum-directory _", this->_input_directories.size());
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
                            path, station, spec::string_to_variable(match[2].str()),
                            year);
                    }
                }
            }
        }
        // skip incomplete records that have less than five files
        for (auto first=files.begin(); first!=files.end(); ) {
            if (complete(first->second)) { ++first; }
            else { first = files.erase(first); }
        }
        this->_num_kernels = files.size();
        for (const auto& pair : files) {
            sbn::upstream<sbn::Remote>(
                this, sbn::make_pointer<Five_files_kernel<T>>(pair.second));
        }
    }

    void react(sbn::kernel_ptr&& child) override {
        sys::log_message("spec", "typeid _ kernel _", typeid(*child.get()).name(), *child.get());
        auto k = sbn::pointer_dynamic_cast<Five_files_kernel<T>>(std::move(child));
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
            sbn::commit<sbn::Remote>(std::move(this_ptr()));
        }
    }

    void read(sbn::kernel_buffer& in) override {
        kernel::read(in);
        in >> _count >> _count_spectra;
        in >> this->_input_directories;
        for (auto& tp : this->_time_points) { in >> tp; }
    }

    void write(sbn::kernel_buffer& out) const override {
        kernel::write(out);
        out << _count << _count_spectra;
        out << this->_input_directories;
        for (const auto& tp : this->_time_points) { out << tp; }
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

public:
    using clock_type = std::chrono::system_clock;
    using time_point = clock_type::time_point;
    using duration = clock_type::duration;

private:
    std::vector<sys::path> _input_directories;
    std::array<time_point,2> _time_points;

public:

    Main() = default;

    inline Main(int argc, char** argv) {
        for (int i=1; i<argc; ++i) {
            this->_input_directories.emplace_back(argv[i]);
        }
    }

    void act() override {
        sys::log_message("spec", "program start");
        auto k = sbn::make_pointer<Spectrum_directory_kernel<T>>(this->_input_directories);
        k->setf(sbn::kernel_flag::carries_parent);
        this->_time_points[0] = clock_type::now();
        sbn::upstream<sbn::Remote>(this, std::move(k));
        if (const char* hostname = std::getenv("SBN_TEST_SUPERIOR_FAILURE")) {
            if (sys::this_process::hostname() == hostname) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                sys::log_message("spec", "simulate superior failure _!", hostname);
                send(sys::signal::kill, sys::this_process::parent_id());
                send(sys::signal::kill, sys::this_process::id());
            }
        }
    }

    void react(sbn::kernel_ptr&&) override {
        this->_time_points[1] = clock_type::now();
        {
            using namespace std::chrono;
            this->_time_points[1] = clock_type::now();
            const auto& t = this->_time_points;
            std::ofstream out("time2.log");
            out << duration_cast<microseconds>(t[1] - t[0]).count() << std::endl;
            out.close();
        }
        sys::log_message("spec", "finished all");
        sbn::commit<sbn::Remote>(std::move(this_ptr()));
    }

    void read(sbn::kernel_buffer& in) override {
        kernel::read(in);
        if (in.remaining() == 0) {
            if (auto* a = target_application()) {
                const auto& args = a->arguments();
                const auto nargs = args.size();
                for (size_t i=1; i<nargs; ++i) {
                    sys::log_message("spec", "arg _", args[i]);
                    this->_input_directories.emplace_back(args[i]);
                }
            }
        } else {
            in >> this->_input_directories;
            for (auto& tp : this->_time_points) { in >> tp; }
        }
    }

    void write(sbn::kernel_buffer& out) const override {
        kernel::write(out);
        out << this->_input_directories;
        for (const auto& tp : this->_time_points) { out << tp; }
    }

};

#endif // vim:filetype=cpp
