#ifndef APPS_DISCOVERY_CSV_ITERATOR_HH
#define APPS_DISCOVERY_CSV_ITERATOR_HH

#include <iterator>
#include <tuple>
#include <istream>
#include <type_traits>
#include <iomanip>

namespace discovery {

	namespace bits {

		template<class Tuple, size_t idx>
		struct read_tuple: public read_tuple<Tuple, idx-1> {

			typedef read_tuple<Tuple, idx-1> base_type;
			using read_tuple<Tuple, idx-1>::_tuple;
			static constexpr const size_t nfields = std::tuple_size<Tuple>::value;

			explicit
			read_tuple(Tuple& rhs):
			base_type(rhs)
			{}

			void
			operator()(std::istream& in) {
//				typedef typename std::tuple_element<idx,value_type>::type element_type;
//				typedef typename std::decay<element_type>::type decay_type;
//				if (std::is_same<decay_type, std::string>::value) {
//					std::getline(*_in, std::get<idx>(_row), _sep);
//				} else
				in >> std::ws >> std::get<nfields-1-idx>(_tuple);
				char ch;
				in >> std::ws >> ch;
				if (ch != ',') in.putback(ch);
				base_type::operator()(in);
			}

		};

		template<class Tuple>
		struct read_tuple<Tuple,0> {
			static constexpr const size_t nfields = std::tuple_size<Tuple>::value;
			explicit
			read_tuple(Tuple& rhs): _tuple(rhs) {}
			void operator()(std::istream& in) {
				in >> std::ws >> std::get<nfields-1>(_tuple);
			}
			Tuple& _tuple;
		};

	}


	struct ignore_field {

		friend std::istream&
		operator>>(std::istream& in, ignore_field& rhs) {
			char ch;
			while (in >> ch and ch != ',' and ch != '\n');
			return in;
		}

		friend std::ostream&
		operator<<(std::ostream& out, const ignore_field& rhs) {
			return out;
		}

		static const char _sep = ',';

	};

	template<class ... Fields>
	struct csv_iterator: public std::iterator<std::input_iterator_tag,std::tuple<Fields...>> {

		typedef std::tuple<Fields...> value_type;
		typedef value_type* pointer;
		typedef value_type& reference;
		typedef std::istream::char_type char_type;

		explicit
		csv_iterator(std::istream& in, char_type sep=',') noexcept:
		_in(&in),
		_sep(sep)
		{}

		csv_iterator() = default;
		csv_iterator(const csv_iterator&) = default;
		csv_iterator& operator=(const csv_iterator&) = default;
		csv_iterator(csv_iterator&&) = default;
		~csv_iterator() = default;

		bool
		operator==(const csv_iterator& rhs) const noexcept {
			return _in->eof();
		}

		bool
		operator!=(const csv_iterator& rhs) const noexcept {
			return !operator==(rhs);
		}

		const reference
		operator*() const noexcept {
			return _row;
		}

		reference
		operator*() noexcept {
			return _row;
		}

		const pointer
		operator->() const noexcept {
			return &_row;
		}

		pointer
		operator->() noexcept {
			return &_row;
		}

		csv_iterator&
		operator++() noexcept {
		std::clog << "operator++" << std::endl;
			bits::read_tuple<value_type,nfields-1> r(_row);
			r(*_in);
			return *this;
		}

		csv_iterator&
		operator++(int) noexcept {
			csv_iterator tmp(*this);
			operator++();
			return tmp;
		}

	private:

		value_type _row;
		std::istream* _in = nullptr;
		char_type _sep = ',';

		static constexpr const size_t nfields = std::tuple_size<value_type>::value;
	};

}

#endif // APPS_DISCOVERY_CSV_ITERATOR_HH
