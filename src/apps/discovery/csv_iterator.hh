#ifndef APPS_DISCOVERY_CSV_ITERATOR_HH
#define APPS_DISCOVERY_CSV_ITERATOR_HH

#include <iterator>
#include <tuple>
#include <istream>
#include <type_traits>
#include <iomanip>

namespace discovery {

	struct ignore_field {

		friend constexpr std::ostream&
		operator<<(std::ostream& out, const ignore_field&) {
			return out;
		}

	};

	namespace bits {

		template<class T>
		void
		read_field(std::istream& in, T& rhs, char) {
			in >> rhs;
		}

		template<>
		void
		read_field<ignore_field>(std::istream& in, ignore_field&, char sep) {
			char ch;
			while (in.get(ch) and ch != sep and ch != '\n');
			if (in) in.putback(ch);
		}

		template<>
		void
		read_field<std::string>(std::istream& in, std::string& rhs, char sep) {
			std::istream::sentry s(in);
			if (s) {
				rhs.clear();
				char ch;
				while (in.get(ch) and ch != sep and ch != '\n') {
					rhs.push_back(ch);
				}
				if (ch == sep) in.putback(ch);
			}
		}

	}

	template<class Tuple, size_t first, size_t last, class Char, Char separator>
	struct basic_csvtuple: public basic_csvtuple<Tuple, first+1, last, Char, separator> {

		typedef basic_csvtuple<Tuple, first+1, last, Char, separator> base_type;

		template<class ... Args2>
		explicit
		basic_csvtuple(Args2&& ... args):
		base_type(std::forward<Args2>(args)...)
		{}

		friend std::istream&
		operator>>(std::istream& in, basic_csvtuple& rhs) {
			bits::read_field(in, std::get<first>(rhs), separator);
			in >> std::ws;
			if (in.get() != separator) in.setstate(std::ios::failbit);
			return operator>>(in, static_cast<base_type&>(rhs));
		}

		friend std::ostream&
		operator<<(std::ostream& out, const basic_csvtuple& rhs) {
			return out << std::get<first>(rhs) << separator
				<< static_cast<const base_type&>(rhs);
		}

	};

	template<class Tuple, size_t last, class Char, Char separator>
	struct basic_csvtuple<Tuple,last,last, Char, separator>: public Tuple {

		template<class ... Args2>
		explicit
		basic_csvtuple(Args2&& ... args):
		Tuple(std::forward<Args2>(args)...)
		{}

		friend std::istream&
		operator>>(std::istream& in, basic_csvtuple& rhs) {
			bits::read_field(in, std::get<last>(rhs), separator);
			return in.ignore(1024*1024, '\n');
		}

		friend std::ostream&
		operator<<(std::ostream& out, const basic_csvtuple& rhs) {
			return out << std::get<last>(rhs);
		}

	};

	template<char separator, class ... Args>
	struct csv_tuple: public basic_csvtuple<std::tuple<Args...>, 0, sizeof...(Args)-1, char, separator> {

		typedef basic_csvtuple<std::tuple<Args...>, 0, sizeof...(Args)-1, char, separator> base_type;

		template<class ... Args2>
		explicit
		csv_tuple(Args2&& ... args):
		base_type(std::forward<Args2>(args)...)
		{}

		friend std::istream&
		operator>>(std::istream& in, csv_tuple& rhs) {
			std::istream::sentry s(in);
			if (s) {
				operator>>(in, static_cast<base_type&>(rhs));
			}
			return in;
		}

	};

}

#endif // APPS_DISCOVERY_CSV_ITERATOR_HH
