namespace factory {

	namespace unix {

		struct file: public unix::fd {

			enum openmode {
				read_only=O_RDONLY,
				write_only=O_WRONLY,
				read_write=O_RDWR
			};

			file() noexcept = default;

			explicit
			file(file&& rhs) noexcept:
				unix::fd(std::move(rhs)) {}

			explicit
			file(const std::string& filename, openmode flags,
				unix::flag_type flags2=0, mode_type mode=S_IRUSR|S_IWUSR):
				unix::fd(components::check(::open(filename.c_str(), flags|flags2, mode),
					__FILE__, __LINE__, __func__)) {}

			~file() {
				this->close();
			}

		};

	}

}
