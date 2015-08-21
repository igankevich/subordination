#ifndef FACTORY_EXT_SHMEMBUF_HH
#define FACTORY_EXT_SHMEMBUF_HH

#include "intro.hh"

namespace factory {

	namespace components {

		template<class Ch, class Tr=std::char_traits<Ch>>
		struct basic_shmembuf: public std::basic_streambuf<Ch,Tr> {

			using typename std::basic_streambuf<Ch,Tr>::int_type;
			using typename std::basic_streambuf<Ch,Tr>::traits_type;
			using typename std::basic_streambuf<Ch,Tr>::char_type;
			using typename std::basic_streambuf<Ch,Tr>::pos_type;
			using typename std::basic_streambuf<Ch,Tr>::off_type;

			typedef typename unix::shared_mem<char_type>::size_type size_type;
			typedef typename unix::shared_mem<char_type>::path_type path_type;
			typedef stdx::spin_mutex mutex_type;
			typedef typename unix::shared_mem<Ch>::proj_id_type proj_id_type;
			typedef stdx::log<basic_shmembuf> this_log;

			explicit
			basic_shmembuf(path_type&& path, unix::mode_type mode):
				_sharedmem(std::forward<path_type>(path), 512, mode, BUFFER_PROJID),
				_sharedpart(new (this->_sharedmem.ptr()) shmem_header)
			{
				this->_sharedpart->size = this->_sharedmem.size();
				char_type* ptr = this->_sharedmem.begin() + sizeof(shmem_header);
				this->setg(ptr, ptr, ptr);
				this->setp(ptr, this->_sharedmem.end());
				this->debug("basic_shmembuf()");
			}

			explicit
			basic_shmembuf(path_type&& path):
				_sharedmem(std::forward<path_type>(path), BUFFER_PROJID),
				_sharedpart(static_cast<shmem_header*>(this->_sharedmem.ptr()))
			{
				char_type* ptr = this->_sharedmem.begin() + sizeof(shmem_header);
				this->setg(ptr, ptr, ptr);
				this->setp(ptr, this->_sharedmem.end());
				this->sync_sharedmem();
				this->debug("basic_shmembuf(int)");
			} 

			basic_shmembuf(basic_shmembuf&& rhs):
				_sharedmem(std::move(rhs._sharedmem)),
				_sharedpart(rhs._sharedpart)
				{}

			basic_shmembuf() = default;
			~basic_shmembuf() = default;

			void open(path_type&& path, unix::mode_type mode) {
				this->_sharedmem.open(std::forward<path_type>(path), 512, mode, BUFFER_PROJID);
				this->_sharedpart = new (this->_sharedmem.ptr()) shmem_header;
				this->_sharedpart->size = this->_sharedmem.size();
				char_type* ptr = this->_sharedmem.begin() + sizeof(shmem_header);
				this->setg(ptr, ptr, ptr);
				this->setp(ptr, this->_sharedmem.end());
				this->writeoffs();
				this->debug("basic_shmembuf::open()");
			}
			
			void attach(path_type&& path) {
				this->_sharedmem.attach(std::forward<path_type>(path), BUFFER_PROJID),
//				this->_sharedpart = new (this->_sharedmem.ptr()) shmem_header;
				this->_sharedpart = static_cast<shmem_header*>(this->_sharedmem.ptr());
				char_type* ptr = this->_sharedmem.begin() + sizeof(shmem_header);
				this->setg(ptr, ptr, ptr);
				this->setp(ptr, this->_sharedmem.end());
				this->sync_sharedmem();
				this->debug("basic_shmembuf::attach()");
			}

			int_type overflow(int_type c = traits_type::eof()) {
//				this->sync_sharedmem();
				int_type ret;
				if (c != traits_type::eof()) {
					this->grow_sharedmem();
					*this->pptr() = c;
					this->pbump(1);
					this->setg(this->eback(), this->gptr(), this->egptr()+1);
//					this->writeoffs();
					ret = traits_type::to_int_type(c);
				} else {
					ret = traits_type::eof();
				}
				return ret;
			}

			int_type underflow() {
				return this->gptr() == this->egptr()
					? traits_type::eof()
					: traits_type::to_int_type(*this->gptr());
			}

			int_type uflow() {
//				this->sync_sharedmem();
				if (this->underflow() == traits_type::eof()) return traits_type::eof();
				int_type c = traits_type::to_int_type(*this->gptr());
				this->gbump(1);
//				this->writeoffs();
				return c;
			}

			std::streamsize xsputn(const char_type* s, std::streamsize n) override {
				this->debug("xsputn");
				std::streamsize nwritten = 0;
				while (nwritten != n) {
					if (this->epptr() == this->pptr()) {
						this->overflow();
					}
					std::streamsize avail = static_cast<std::streamsize>(this->epptr() - this->pptr());
					std::streamsize m = std::min(n, avail);
					traits_type::copy(this->pptr(), s, m);
					nwritten += m;
					this->pbump(m);
				}
				return nwritten;
			}

			void lock() {
				this->mutex().lock();
				this->sync_sharedmem();
				this->debug("lock");
			}

			void unlock() {
				this->writeoffs();
				this->mutex().unlock();
				this->debug("unlock");
			}

		private:

			mutex_type& mutex() { return this->_sharedpart->mtx; }

			void debug(const char* msg) {
				this_log()
					<< msg << ": size1="
					<< this->_sharedpart->size
					<< ",goff=" << this->_sharedpart->goff
					<< ",poff=" << this->_sharedpart->poff
					<< ",pptr=" << static_cast<std::ptrdiff_t>(this->pptr() - this->pbase())
					<< ",shmem="
					<< this->_sharedmem
					<< std::endl;
			}

			void grow_sharedmem() {
				this->_sharedmem.resize(this->_sharedmem.size()
					* size_type(2));
				this->_sharedpart->size = this->_sharedmem.size();
				this->updatebufs();
			}
			
			void sync_sharedmem() {
				// update shared memory size
				if (this->bad_size()) {
					this->_sharedmem.sync();
//					this->_sharedpart->size = this->_sharedmem.size();
				}
				this->debug("sync_sharedmem");
				if (this->bad_size()) {
					throw Error("bad unix::shared_mem size",
						__FILE__, __LINE__, __func__);
				}
				this->readoffs();
			}
			
			bool bad_size() const {
				return this->_sharedpart->size
					!= this->_sharedmem.size();
			}
			
			void updatebufs() {
				std::ptrdiff_t poff = this->pptr() - this->pbase();
				std::ptrdiff_t goff = this->gptr() - this->eback();
				char_type* base = this->_sharedmem.begin() + sizeof(shmem_header);
				char_type* end = this->_sharedmem.end();
				this->setp(base, end);
				this->pbump(poff);
				this->setg(base, base + goff, base + poff);
			}

			void writeoffs() {
				this->_sharedpart->goff = static_cast<pos_type>(this->gptr() - this->eback());
				this->_sharedpart->poff = static_cast<pos_type>(this->pptr() - this->pbase());
				this->debug("writeoffs");
			}

			void readoffs() {
				pos_type goff = this->_sharedpart->goff;
				pos_type poff = this->_sharedpart->poff;
				char_type* base = this->_sharedmem.begin() + sizeof(shmem_header);
				char_type* end = this->_sharedmem.end();
				this->setp(base, end);
				this->pbump(poff);
				this->setg(base, base + goff, base + poff);
				this->debug("readoffs");
			}

			unix::shared_mem<char_type> _sharedmem;
			struct shmem_header {
				size_type size = 0;
				mutex_type mtx;
				pos_type goff = 0;
				pos_type poff = 0;
			} *_sharedpart = nullptr;

			static const proj_id_type BUFFER_PROJID = 'b';
		};

	}

	namespace stdx {

		template<class Ch, class Tr>
		struct type_traits<components::basic_shmembuf<Ch,Tr>> {
			static constexpr const char*
			short_name() { return "shmembuf"; }
			typedef components::buffer_category category;
		};
	
	}

}
#endif // FACTORY_EXT_SHMEMBUF_HH
