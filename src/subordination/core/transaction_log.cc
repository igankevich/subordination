#include <memory>

#include <subordination/core/application.hh>
#include <subordination/core/foreign_kernel.hh>
#include <subordination/core/kernel.hh>
#include <subordination/core/transaction_log.hh>

namespace  {
    template <class ... Args>
    inline void
    log(const Args& ... args) {
        sys::log_message("log", args ...);
    }
}

sbn::transaction_log::~transaction_log() { close(); }

void sbn::transaction_log::write(transaction_status status,
                                 pipeline::index_type pipeline_index,
                                 kernel* k) {
    lock_type lock(this->_mutex);
    {
        kernel_frame frame;
        kernel_write_guard g(frame, this->_buffer);
        this->_buffer.write(status);
        this->_buffer.write(pipeline_index);
        if (status == transaction_status::end) {
            this->_buffer.write(k->id());
        } else {
            this->_buffer.write(k);
        }
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

void sbn::transaction_log::open(const char* filename) {
    using f = sys::open_flag;
    this->_file_descriptor.open(filename, f::append | f::close_on_exec | f::create |
                                f::write_only | f::dsync | f::direct, 0700);
    const auto offset = this->_file_descriptor.offset();
    if (offset != 0) { return recover(filename, offset); }
}

void sbn::transaction_log::recover(const char* filename, sys::offset_type max_offset) {
    using f = sys::open_flag;
    auto& buf = this->_buffer;
    sys::fildes fd(filename, f::close_on_exec | f::read_only);
    struct record {
        pipeline::index_type pipeline_index;
        kernel* k;
        record(pipeline::index_type i, kernel* kk): pipeline_index(i), k(kk) {}
    };
    std::vector<record> records;
    auto erase_upstream = [&] (kernel::id_type id) {
        auto first = records.begin(), last = records.end();
        while (first != last) {
            if (first->k->id() == id) { first = records.erase(first); }
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
            pipeline::index_type pipeline_index{};
            kernel* k = nullptr;
            buf.read(status);
            buf.read(pipeline_index);
            if (status == transaction_status::end) {
                kernel::id_type id = 0;
                buf.read(id);
                erase_upstream(k->id());
            } else {
                // TODO add recursive flag to kernel buffer
                // when enabled, the buffer saves all hierarchy of parent kernels,
                // not just the closest one
                buf.read(k);
                records.emplace_back(pipeline_index,k);
            }
        }
        buf.compact();
    }
    fd.close();
    for (auto& r : records) {
        if (r.pipeline_index >= this->_pipelines.size()) {
            log("wrong pipeline index _, deleting _", r.pipeline_index, r.k);
            delete r.k;
        }
        this->_pipelines[r.pipeline_index]->send(r.k);
    }
}

void sbn::transaction_log::update_pipeline_indices() {
    auto n = static_cast<pipeline::index_type>(this->_pipelines.size());
    for (pipeline::index_type i=0; i<n; ++i) { this->_pipelines[i]->index(i); }
}
