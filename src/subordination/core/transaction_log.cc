#include <memory>

#include <subordination/core/application.hh>
#include <subordination/core/foreign_kernel.hh>
#include <subordination/core/kernel.hh>
#include <subordination/core/transaction_log.hh>

void sbn::transaction_log::write(transaction_status status, kernel* k) {
    {
        kernel_frame frame;
        kernel_write_guard g(frame, this->_buffer);
        this->_buffer.write(status);
        this->_buffer.write(k);
    }
    flush();
}

void sbn::transaction_log::flush() {
    this->_buffer.flush(this->_file_descriptor);
    this->_buffer.compact();
}

void sbn::transaction_log::close() {
    if (!this->_file_descriptor) { return; }
    flush();
    this->_file_descriptor.close();
}

auto sbn::transaction_log::open(const char* filename) -> kernel_array {
    using f = sys::open_flag;
    this->_file_descriptor.open(filename, f::append | f::close_on_exec | f::create |
                                f::write_only | f::dsync | f::direct, 0700);
    auto offset = ::lseek(this->_file_descriptor.fd(), 0, SEEK_CUR);
    UNISTDX_CHECK(offset);
    if (offset != 0) { return recover(filename, offset); }
    return {};
}

auto sbn::transaction_log::recover(const char* filename, sys::offset_type max_offset)
-> kernel_array {
    using f = sys::open_flag;
    auto& buf = this->_buffer;
    sys::fildes fd(filename, f::close_on_exec | f::read_only);
    kernel_array kernels;
    auto erase_upstream = [&] (kernel::id_type id) {
        auto first = kernels.begin(), last = kernels.end();
        while (first != last) {
            if ((*first)->id() == id) { first = kernels.erase(first); }
            else { ++first; }
        }
    };
    kernel_frame frame;
    struct fd_reader {
        sys::fildes& fd;
        ::off_t offset;
        ::off_t max_offset;
        ssize_t read(void* dst, size_t n) {
            n = std::min(n, size_t(max_offset-offset));
            if (n == 0) { return 0; }
            auto nread = fd.read(dst, n);
            if (nread != -1) { offset += nread; }
            return nread;
        }
    };
    fd_reader reader{fd,0,max_offset};
    while (reader.offset != reader.max_offset) {
        buf.fill(fd);
        buf.flip();
        while (buf.remaining() >= sizeof(kernel_frame)) {
            kernel_read_guard g(frame, buf);
            if (!g) { break; }
            transaction_status status{};
            kernel* k = nullptr;
            buf.read(status);
            // TODO add recursive flag to kernel buffer
            // when enabled, the buffer saves all hierarchy of parent kernels,
            // not just the closest one
            buf.read(k);
            if (status == transaction_status::end) {
                erase_upstream(k->id());
            } else {
                kernels.emplace_back(k);
            }
        }
        buf.compact();
    }
    fd.close();
    return kernels;
}
