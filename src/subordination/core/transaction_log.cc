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
    // N.B. We use embedded frame here, because foreign kernels read their payload
    // up to the end of the frame.
    kernel_frame frame;
    kernel_write_guard g(frame, out);
    out.write(rhs.status);
    out.write(rhs.pipeline_index);
    out.write(rhs.k.get());
    return out;
}

sbn::kernel_buffer& sbn::operator>>(kernel_buffer& in, transaction_record& rhs) {
    // N.B. We use embedded frame here, because foreign kernels read their payload
    // up to the end of the frame.
    kernel_frame frame;
    kernel_read_guard g(frame, in);
    in.read(rhs.status);
    in.read(rhs.pipeline_index);
    in.read(rhs.k);
    return in;
}

sbn::transaction_log::transaction_log() { this->_buffer.carry_all_parents(false); }

sbn::transaction_log::~transaction_log() noexcept {
    try {
        close();
    } catch (...) {
        std::terminate();
    }
}

sbn::kernel_ptr sbn::transaction_log::write(transaction_record record) {
    if (!record.k->carries_parent()) { return std::move(record.k); }
    lock_type lock(this->_mutex);
    this->_buffer << record;
    this->_buffer.flip();
    log("flush _", this->_buffer.remaining());
    this->_buffer.flush(this->_file_descriptor);
    this->_buffer.compact();
    log("store _ _", *record.k, _file_descriptor.offset());
    return std::move(record.k);
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
                                f::close_on_exec | f::create | f::read_write /*| f::dsync*/,
                                0600);
    this->_file_descriptor.offset(0, sys::seek_origin::end);
    const auto offset = this->_file_descriptor.offset();
    log("file _ offset _", filename, offset);
    if (offset != 0) {
        this->_file_descriptor.close();
        return recover(filename, offset);
    }
}

auto sbn::transaction_log::select(application::id_type id) -> record_array {
    lock_type lock(this->_mutex);
    offset_guard g(this->_file_descriptor);
    this->_file_descriptor.offset(0);
    kernel_buffer buf{4096};
    buf.types(this->_buffer.types());
    buf.carry_all_parents(false);
    auto records = read_records(this->_file_descriptor, g.old_offset(), buf);
    auto first = records.begin(), last = records.end();
    while (first != last) {
        auto& r = *first;
        if (!r.k || (r.k->source_application_id() != id &&
                     r.k->target_application_id() != id)) {
            first = records.erase(first);
            last = records.end();
        } else { ++first; }
    }
    return records;
}

auto sbn::transaction_log::read_records(sys::fildes& fd,
                                        sys::offset_type max_offset,
                                        kernel_buffer& buf)
-> record_array {
    std::vector<transaction_record> records;
    auto erase_upstream = [&] (kernel::id_type id) {
        auto first = records.begin(), last = records.end();
        while (first != last) {
            if (first->k->id() == id) {
                first = records.erase(first);
                last = records.end();
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
            kernel_ptr k;
            buf.read(status);
            buf.read(pipeline_index);
            if (status == transaction_status::end) {
                kernel::id_type id = 0;
                buf.read(id);
                erase_upstream(id);
            } else {
                buf.read(k);
                records.emplace_back(status, pipeline_index, std::move(k));
            }
        }
        buf.compact();
    }
    return records;
}

void sbn::transaction_log::plug_parents(record_array& records) {
    std::unordered_map<kernel::id_type,kernel*> parents;
    for (auto& r : records) {
        if (!r.k) { continue; }
        auto* p = r.k.get();
        while (p) {
            parents[p->id()] = p;
            p = p->parent();
        }
    }
    for (auto& r : records) {
        if (!r.k) { continue; }
        auto result = parents.find(r.k->parent_id());
        if (result != parents.end()) {
            r.k->parent(result->second);
        }
    }
}

void sbn::transaction_log::recover(record_array& records) {
    // send recovered kernels to the respective pipelines
    for (auto& r : records) {
        if (r.k) {
            log("restore _", *r.k);
        }
    }
    for (auto& r : records) {
        if (r.k) {
            if (r.pipeline_index >= this->_pipelines.size()) {
                log("wrong pipeline index _, deleting _", r.pipeline_index, r.k.get());
                r.k.reset();
            }
            auto* ppl = this->_pipelines[r.pipeline_index];
            if (r.k->is_native()) {
                // TODO add "failed" kernel phase in which rollback() is called instead of
                // act() or react().
                ppl->send(std::move(r.k));
            } else {
                ppl->forward(std::move(r.k));
            }
        }
    }
    // TODO clean log periodically
}

void sbn::transaction_log::recover(const char* filename, sys::offset_type max_offset) {
    log("recover");
    using f = sys::open_flag;
    sys::fildes fd(filename, f::close_on_exec | f::read_only);
    auto& buf = this->_buffer;
    auto records = read_records(fd, max_offset, buf);
    plug_parents(records);
    kernel_buffer new_buffer{4096};
    new_buffer.types(this->_buffer.types());
    new_buffer.carry_all_parents(false);
    fd.close();
    buf.clear();
    if (records.size() > max_records()) {
        actual_records(records.size());
        log("restore _/_ records", max_records(), actual_records());
        records.resize(max_records());
    }
    for (auto& r : records) { new_buffer << r; }
    std::string new_name;
    new_name += filename;
    new_name += ".new";
    sys::fildes new_fd(new_name.data(),
                       f::close_on_exec | f::create | f::write_only /*| f::dsync*/ | f::truncate,
                       0600);
    new_buffer.flip();
    new_buffer.flush(new_fd);
    new_buffer.compact();
    new_fd.close();
    std::rename(new_name.data(), filename);
    this->_file_descriptor.open(filename, f::close_on_exec | f::create |
                                f::write_only /*| f::dsync*/, 0600);
    this->_file_descriptor.offset(0, sys::seek_origin::end);
    if (this->_recover_after == duration::zero()) {
        recover(records);
    } else {
        auto k = sbn::make_pointer<transaction_kernel>();
        k->records(std::move(records));
        k->transactions(this);
        k->after(this->_recover_after);
        if (this->_timer_pipeline) { this->_timer_pipeline->send(std::move(k)); }
    }
}

void sbn::transaction_log::update_pipeline_indices() {
    auto n = static_cast<pipeline::index_type>(this->_pipelines.size());
    for (pipeline::index_type i=0; i<n; ++i) { this->_pipelines[i]->index(i); }
}

void sbn::transaction_kernel::act() {
    this->_transactions->recover(this->_records);
    this_ptr().reset();
}

bool sbn::transaction_log::properties::set(const char* key, const std::string& value) {
    bool found = true;
    if (std::strcmp(key, "directory") == 0) {
        directory = value;
    } else if (std::strcmp(key, "recover-after") == 0) {
        recover_after = sbn::string_to_duration(value);
    } else {
        found = false;
    }
    return found;
}
