#ifndef EXAMPLES_CPP_SPEC_GZIP_FILE_HH
#define EXAMPLES_CPP_SPEC_GZIP_FILE_HH

#include <zlib.h>

#include <sstream>
#include <stdexcept>

#include <subordination/core/error.hh>

namespace spec {

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
                std::stringstream tmp;
                tmp << "unable to open " << filename;
                throw std::invalid_argument(tmp.str());
            }
        }

        inline void close() {
            ::gzclose(this->_file);
            this->_file = nullptr;
        }

        inline int read(void* ptr, unsigned len) {
            int ret = ::gzread(this->_file, ptr, len);
            if (ret == -1) { sbn::throw_error("i/o error"); }
            return ret;
        }

    };

    inline void swap(GZIP_file& a, GZIP_file& b) { a.swap(b); }

}

#endif // vim:filetype=cpp
