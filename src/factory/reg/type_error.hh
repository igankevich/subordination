#ifndef FACTORY_REG_TYPE_ERROR_HH
#define FACTORY_REG_TYPE_ERROR_HH

#include <factory/error.hh>
#include "type.hh"

namespace factory {

	class Type_error: public Error {

	private:
		Type _tp1, _tp2;

	public:
		inline
		Type_error(const Type& tp1, const Type& tp2):
		Error("types have the same type indices/identifiers"),
		_tp1(tp1),
		_tp2(tp2)
		{}

		friend std::ostream&
		operator<<(std::ostream& out, const Type_error& rhs);

	};

	std::ostream&
	operator<<(std::ostream& out, const Type_error& rhs);

}

#endif // vim:filetype=cpp
