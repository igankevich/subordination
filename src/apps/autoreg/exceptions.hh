#ifndef APPS_AUTOREG_EXCEPTIONS_HH
#define APPS_AUTOREG_EXCEPTIONS_HH

#pragma once
#include <string>

namespace autoreg {

//class Error: public std::exception {
//	std::string description;
//public:
//	Error(std::string name): std::exception(), description(name) {}
//	~Error() throw() {}
//	const char* what() const throw() { return description.c_str(); }
//};


class File_not_found: public std::exception {
	std::string filename;
public:
	File_not_found(std::string name): std::exception(), filename(name) {}
	~File_not_found() throw() {}
	const char* what() const throw() { return ("File not found: " + filename).c_str(); }
};

}

#endif // APPS_AUTOREG_EXCEPTIONS_HH
