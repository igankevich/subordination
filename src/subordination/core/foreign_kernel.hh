#ifndef SUBORDINATION_CORE_FOREIGN_KERNEL_HH
#define SUBORDINATION_CORE_FOREIGN_KERNEL_HH

#include <memory>

#include <subordination/core/kernel.hh>
#include <subordination/core/kernel_type.hh>
#include <subordination/core/types.hh>

namespace sbn {

    class foreign_kernel: public kernel {

    private:
        sys::u32 _size = 0;
        std::unique_ptr<char[]> _payload{};

    public:
        foreign_kernel() = default;
        foreign_kernel(foreign_kernel&&) = delete;
        foreign_kernel& operator=(foreign_kernel&&) = delete;
        foreign_kernel(const foreign_kernel&) = delete;
        foreign_kernel& operator=(const foreign_kernel&) = delete;
        virtual ~foreign_kernel() = default;

        inline bool is_native() const noexcept override { return false; }

        void read(kernel_buffer& in) override;
        void write(kernel_buffer& out) const override;

    };

}

#endif // vim:filetype=cpp
