#ifndef SYSX_LOG_COLOR_HH
#define SYSX_LOG_COLOR_HH

#include <unistd.h>

namespace sysx {

	enum struct log_color {
		RESET            = 0,
		FG_RED           = 31,
		FG_GREEN         = 32,
		FG_YELLOW        = 33,
		FG_BLUE          = 34,
		FG_MAGENTA       = 35,
		FG_CYAN          = 36,
		FG_LIGHT_GRAY    = 37,
		FG_DEFAULT       = 39,
		BG_RED           = 41,
		BG_GREEN         = 42,
		BG_BLUE          = 44,
		BG_DEFAULT       = 49,
		FG_DARK_GRAY     = 90,
		FG_LIGHT_RED     = 91,
		FG_LIGHT_GREEN   = 92,
		FG_LIGHT_YELLOW  = 93,
		FG_LIGHT_BLUE    = 94,
		FG_LIGHT_MAGENTA = 95,
		FG_LIGHT_CYAN    = 96,
		FG_WHITE         = 97
	};

	bool
	log_goes_to_terminal() noexcept {
		return static_cast<bool>(::isatty(2));
	}

	std::ostream&
	operator<<(std::ostream& os, log_color rhs) {
		if (log_goes_to_terminal()) {
			os << "\033[" << static_cast<int>(rhs) << "m";
		}
		return os;
	}

}

#endif // SYSX_LOG_COLOR_HH
