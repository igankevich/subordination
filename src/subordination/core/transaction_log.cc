#include <memory>
#include <unordered_map>

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

sbn::kernel_buffer& sbn::operator<<(kernel_buffer& out, transaction_status rhs) {
    kernel_frame frame;
    kernel_write_guard g(frame, out);
    out.write(rhs);
    return out;
}

sbn::kernel_buffer& sbn::operator<<(kernel_buffer& out, const transaction_record& rhs) {
    kernel_frame frame;
    kernel_write_guard g(frame, out);
    out.write(rhs.status);
    out.write(rhs.pipeline_index);
    if (rhs.status == transaction_status::end) {
        out.write(rhs.k->id());
    } else {
        out.write(rhs.k);
    }
    return out;
}

sbn::transaction_log::transaction_log() { this->_buffer.carry_all_parents(false); }
sbn::transaction_log::~transaction_log() { close(); }

void sbn::transaction_log::write(const transaction_record& record) {
    lock_type lock(this->_mutex);
    this->_buffer << record;
    this->_buffer.flip();
    this->_buffer.flush(this->_file_descriptor);
    this->_buffer.compact();
    log("store _ _", *record.k, _file_descriptor.offset());
}

void sbn::transaction_log::flush() {
    this->_buffer.flip();
    this->_buffer.flush(this->_file_descriptor);
    this->_buffer.compact();
}

void sbn::transaction_log::close() {
    //if (!this->_file_descriptor) { return; }
    //flush();
    this->_file_descriptor.close();
}

void sbn::transaction_log::open(const char* filename) {
    using f = sys::open_flag;
    this->_file_descriptor.open(filename,
                                f::close_on_exec | f::create | f::write_only | f::dsync,
                                0600);
    this->_file_descriptor.offset(0, sys::seek_origin::end);
    const auto offset = this->_file_descriptor.offset();
    log("file _ offset _", filename, offset);
    if (offset != 0) {
        this->_file_descriptor.close();
        return recover(filename, offset);
    }
}

void sbn::transaction_log::recover(const char* filename, sys::offset_type max_offset) {
    log("recover");
    using f = sys::open_flag;
    auto& buf = this->_buffer;
    sys::fildes fd(filename, f::close_on_exec | f::read_only);
    kernel_buffer new_buffer{4096};
    new_buffer.types(this->_buffer.types());
    new_buffer.carry_all_parents(false);
    std::unordered_map<kernel::id_type,kernel*> parents;
    std::vector<transaction_record> records;
    auto erase_upstream = [&] (kernel::id_type id) {
        auto first = records.begin(), last = records.end();
        while (first != last) {
            if (first->k->id() == id) {
                delete first->k;
                first = records.erase(first);
            } else { ++first; }
        }
    };
    kernel_frame frame;
    struct fd_reader {
        sys::fildes& fd;
        sys::offset_type offset;
        sys::offset_type max_offset;
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
        buf.fill(reader);
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
                erase_upstream(id);
            } else {
                // TODO add recursive flag to kernel buffer
                // when enabled, the buffer saves all hierarchy of parent kernels,
                // not just the closest one
                buf.read(k);
                records.emplace_back(status, pipeline_index, k);
                auto* p = k;
                while (p) {
                    parents[p->id()] = p;
                    p = p->parent();
                }
            }
        }
        buf.compact();
    }
    fd.close();
    buf.clear();
    for (auto& r : records) {
        if (r.pipeline_index >= this->_pipelines.size()) {
            log("wrong pipeline index _, deleting _", r.pipeline_index, r.k);
            delete r.k;
            r.k = nullptr;
        } else if (!r.k->carries_parent()) {
            log("does not carry parent, deleting _", r.k);
            delete r.k;
            r.k = nullptr;
        } else {
            new_buffer << r;
        }
    }
    std::string new_name;
    new_name += filename;
    new_name += ".new";
    sys::fildes new_fd(new_name.data(),
                       f::close_on_exec | f::create | f::write_only | f::dsync | f::truncate,
                       0600);
    new_buffer.flip();
    new_buffer.flush(new_fd);
    new_buffer.compact();
    new_fd.close();
    std::rename(new_name.data(), filename);
    this->_file_descriptor.open(filename, f::close_on_exec | f::create |
                                f::write_only | f::dsync, 0600);
    this->_file_descriptor.offset(0, sys::seek_origin::end);
    // send recovered kernels to the respective pipelines
    for (auto& r : records) {
        if (r.k) {
            log("restore _", *r.k);
        }
    }
    for (auto& r : records) {
        if (r.k) {
            auto result = parents.find(r.k->parent_id());
            if (result != parents.end()) {
                r.k->parent(result->second);
            }
            this->_pipelines[r.pipeline_index]->send(r.k);
        }
    }
    // TODO clean log periodically
}

void sbn::transaction_log::update_pipeline_indices() {
    auto n = static_cast<pipeline::index_type>(this->_pipelines.size());
    for (pipeline::index_type i=0; i<n; ++i) { this->_pipelines[i]->index(i); }
}
