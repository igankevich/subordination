#ifndef FACTORY_UNISTDX_CMDLINE_HH
#define FACTORY_UNISTDX_CMDLINE_HH

#include <algorithm>
#include <iterator>
#include <string>
#include <istream>
#include <iomanip>
#include <sstream>

namespace factory {

	namespace unix {

		struct Command_line {

			explicit
			Command_line(int argc, char* argv[]) noexcept:
			_cmdline()
			{
				std::copy_n(argv, argc,
					std::ostream_iterator<char*>(_cmdline, "\n"));
			}

			template<class F>
			void
			parse(F handle) {
				std::string arg;
				while (_cmdline >> std::ws >> arg) {
					handle(arg, _cmdline);
				}
			}

		private:
			std::stringstream _cmdline;
		};

	}

}

#endif // FACTORY_UNISTDX_CMDLINE_HH
