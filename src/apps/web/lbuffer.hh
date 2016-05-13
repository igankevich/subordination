#ifndef APPS_WEB_LBUFFER_HH
#define APPS_WEB_LBUFFER_HH

#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>

namespace factory {

	// TODO: remove this class
	template<class T>
	struct LBuffer {

		typedef T value_type;
		typedef size_t Size;

		explicit LBuffer(size_t base_size): _data(base_size) {}

		void push_back(const T byte) {
			if (write_pos + 1 == buffer_size()) { double_size(); }
			_data[write_pos] = byte;
			++write_pos;
		}

		const T& front() const { return _data[read_pos]; }
		T& front() { return _data[read_pos]; }

		void pop_front() {
			if (!empty()) {
				advance_read_pos(1);
			}
		}

		size_t write(const T* buf, size_t sz) {
			while (write_pos + sz >= buffer_size()) { double_size(); }
			std::copy(buf, buf + sz, &_data[write_pos]);
			write_pos += sz;
			return sz;
		}

		size_t read(T* buf, size_t sz) {
			size_t min_sz = std::min(sz, size());
			std::copy(&_data[read_pos], &_data[read_pos + min_sz], buf);
			advance_read_pos(min_sz);
			return min_sz;
		}

		size_t skip(size_t n) {
			size_t min_n = std::min(n, size());
			advance_read_pos(min_n);
			return min_n;
		}

		size_t readsome(T* buf, size_t sz) {
			size_t min_sz = std::min(sz, size());
			std::copy(&_data[read_pos], &_data[read_pos + min_sz], buf);
			return min_sz;
		}

		size_t ignore(size_t nbytes) {
			size_t min_sz = std::min(nbytes, size());
			advance_read_pos(min_sz);
			return min_sz;
		}

		void reset() {
			read_pos = 0;
			write_pos = 0;
		}

		bool empty() const { return size() == 0; }
		size_t size() const { return write_pos - read_pos; }
		size_t buffer_size() const { return _data.size(); }
		size_t write_size() const { return buffer_size() - write_pos; }

		T* write_begin() { return &_data[write_pos]; }
		T* write_end() { return &_data[buffer_size()]; }

		T* read_begin() { return &_data[read_pos]; }
		T* read_end() { return &_data[write_pos]; }

		T* begin() { return read_begin(); }
		T* end() { return read_end(); }

		const T* begin() const { return &_data[read_pos]; }
		const T* end() const { return &_data[write_pos]; }

		template<class S>
		size_t flush(S sink) {
			size_t bytes_written;
			size_t total = read_pos;
			while ((bytes_written = sink.write(read_begin(), write_pos-read_pos)) > 0) {
				read_pos += bytes_written;
			}
			total = read_pos - total;
			if (read_pos == write_pos) {
				reset();
			}
			return total;
		}

		template<class S>
		size_t fill(S source) {
			size_t bytes_read;
			size_t old_pos = write_pos;
			while ((bytes_read = source.read(write_begin(), write_size())) > 0) {
				write_pos += bytes_read;
				if (write_pos == buffer_size()) {
					double_size();
				}
			}
			return write_pos - old_pos;
		}

		friend std::ostream& operator<<(std::ostream& out, const LBuffer<T>& rhs) {
			out << std::hex << std::setfill('0');
			size_t max_elems = out.width() / 2;
			auto last = std::min(rhs.begin() + max_elems, rhs.end());
			std::for_each(rhs.begin(), last, [&out] (T x) {
				out << std::setw(2) << to_binary(x);
			});
			if (last != rhs.end()) {
				out << "...(truncated)";
			}
			return out;
		}

	private:
		static unsigned int to_binary(T x) {
			return static_cast<unsigned int>(static_cast<unsigned char>(x));
		}

		void advance_read_pos(size_t amount) {
			read_pos += amount;
			if (read_pos == write_pos) {
				reset();
			}
		}

		void double_size() { _data.resize(buffer_size()*2u); }

		std::vector<T> _data;
		size_t write_pos = 0;
		size_t read_pos = 0;
	};

}

#endif // APPS_WEB_LBUFFER_HH
