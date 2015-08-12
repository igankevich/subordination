namespace factory {

	namespace components {

		template<class T>
		struct shared_mem {
		
			typedef uint8_t proj_id_type;
			typedef ::key_t key_type;
			typedef size_t size_type;
			typedef unsigned short perms_type;
			typedef uint64_t id_type;
			typedef int shmid_type;
			typedef void* addr_type;
			typedef T value_type;
		
			shared_mem(id_type id, size_type sz, proj_id_type num, perms_type perms = DEFAULT_SHMEM_PERMS):
				_id(id), _size(sz), _key(this->genkey(num)),
				_shmid(this->createshmem(perms)),
				_addr(this->attach()), _owner(true)
			{
				this->fillshmem();
				Logger<Level::SHMEM>()
					<< "shared_mem server: "
					<< "shmem=" << *this
					<< std::endl;
			}
		
			shared_mem(id_type id, proj_id_type num):
				_id(id), _size(0), _key(this->genkey(num)),
				_shmid(this->getshmem()),
				_addr(this->attach())
			{
				this->_size = this->load_size();
				Logger<Level::SHMEM>()
					<< "shared_mem client: "
					<< "shmem=" << *this
					<< std::endl;
			}

			shared_mem(shared_mem&& rhs):
				_id(rhs._id),
				_size(rhs._size),
				_key(rhs._key),
				_shmid(rhs._shmid),
				_addr(rhs._addr),
				_owner(rhs._owner)
			{
				rhs._addr = nullptr;
				rhs._owner = false;
			}

			shared_mem() = default;
		
			~shared_mem() {
				this->detach();
				if (this->is_owner()) {
					this->rmshmem();
					this->rmfile();
				}
			}
		
			shared_mem(const shared_mem&) = delete;
			shared_mem& operator=(const shared_mem&) = delete;
		
			addr_type ptr() { return this->_addr; }
			const addr_type ptr() const { return this->_addr; }
			size_type size() const { return this->_size; }
			bool is_owner() const { return this->_owner; }

			value_type* begin() { return static_cast<value_type*>(this->_addr); }
			value_type* end() { return static_cast<value_type*>(this->_addr) + this->_size; }
		
			const value_type* begin() const { return static_cast<value_type*>(this->_addr); }
			const value_type* end() const { return static_cast<value_type*>(this->_addr) + this->_size; }

			void resize(size_type new_size) {
				if (new_size > this->_size) {
					this->detach();
					if (this->is_owner()) {
						this->rmshmem();
					}
					this->_size = new_size;
					this->_shmid = this->createshmem(this->getperms());
					this->attach();
				}
			}

			void sync() {
				shmid_type newid = this->getshmem();
				if (newid != this->_shmid) {
					Logger<Level::SHMEM>()
						<< "detach/attach sync"
						<< std::endl;
					this->detach();
					if (this->is_owner()) {
						this->rmshmem();
					}
					this->_shmid = newid;
					this->attach();
				}
				this->_size = this->load_size();
			}

			void create(id_type id, size_type sz, proj_id_type num, perms_type perms = DEFAULT_SHMEM_PERMS) {
				this->_id = id;
				this->_size = sz;
				this->_key = this->genkey(num);
				this->_shmid = this->createshmem(perms);
				this->_addr = this->attach();
				this->_owner = true;
				this->fillshmem();
				Logger<Level::SHMEM>()
					<< "shared_mem server: "
					<< "shmem=" << *this
					<< std::endl;
			}

			void attach(id_type id, proj_id_type num) {
				this->_id = id;
				this->_size = 0;
				this->_key = this->genkey(num);
				this->_shmid = this->getshmem();
				this->_addr = this->attach();
				this->_size = this->load_size();
				Logger<Level::SHMEM>()
					<< "shared_mem client: "
					<< "shmem=" << *this
					<< std::endl;
			}

		private:

			void fillshmem() {
				std::fill_n(this->begin(),
					this->size(), value_type());
			}

			key_type genkey(proj_id_type num) const {
				std::string path = filename(this->_id);
				{ std::filebuf out; out.open(path, std::ios_base::out); }
				return check(::ftok(path.c_str(), num),
					__FILE__, __LINE__, __func__);
			}

			shmid_type createshmem(perms_type perms) const {
				return check(::shmget(this->_key,
					this->size(), perms | IPC_CREAT),
					__FILE__, __LINE__, __func__);
			}

			shmid_type getshmem() const {
				return check(::shmget(this->_key, 0, 0),
					__FILE__, __LINE__, __func__);
			}

			addr_type attach() const {
				return check(::shmat(this->_shmid, 0, 0),
					__FILE__, __LINE__, __func__);
			}

			void detach() {
				if (this->_addr) {
					check(::shmdt(this->_addr),
						__FILE__, __LINE__, __func__);
				}
			}

			void rmshmem() {
				check(::shmctl(this->_shmid, IPC_RMID, 0),
					__FILE__, __LINE__, __func__);
			}

			void rmfile() {
				std::string path = this->filename(this->_id);
				Logger<Level::SHMEM>()
					<< "rmfile path=" << path
					<< ",shmem=" << *this
					<< std::endl;
				check(::remove(path.c_str()),
					__FILE__, __LINE__, __func__);
			}

			size_type load_size() {
				::shmid_ds stat;
				check(::shmctl(_shmid, IPC_STAT, &stat),
					__FILE__, __LINE__, __func__);
				return stat.shm_segsz;
			}

			perms_type getperms() {
				::shmid_ds stat;
				check(::shmctl(_shmid, IPC_STAT, &stat),
					__FILE__, __LINE__, __func__);
				return stat.shm_perm.mode;
			}
			
			static
			std::string filename(id_type id) {
				std::ostringstream path;
				path << "/var/tmp";
				path << "/factory.";
				path << std::hex << id;
				path << ".shmem";
				return path.str();
			}
		
			friend std::ostream& operator<<(std::ostream& out, const shared_mem& rhs) {
				return out << "{addr=" << rhs.ptr()
					<< ",size=" << rhs.size()
					<< ",owner=" << rhs.is_owner()
					<< ",key=" << rhs._key
					<< ",shmid=" << rhs._shmid
					<< '}';
			}
		
			id_type _id = 0;
			size_type _size = 0;
			key_type _key = 0;
			shmid_type _shmid = 0;
			addr_type _addr = nullptr;
			bool _owner = false;
		
			static const perms_type DEFAULT_SHMEM_PERMS = 0666;
		};

		template<class Ch, class Tr=std::char_traits<Ch>>
		struct basic_shmembuf: public std::basic_streambuf<Ch,Tr> {

			using typename std::basic_streambuf<Ch,Tr>::int_type;
			using typename std::basic_streambuf<Ch,Tr>::traits_type;
			using typename std::basic_streambuf<Ch,Tr>::char_type;
			using typename std::basic_streambuf<Ch,Tr>::pos_type;
			using typename std::basic_streambuf<Ch,Tr>::off_type;
			typedef std::ios_base::openmode openmode;
			typedef std::ios_base::seekdir seekdir;
			typedef typename shared_mem<char_type>::size_type size_type;
			typedef typename shared_mem<char_type>::id_type id_type;
			typedef spin_mutex mutex_type;
//			typedef std::lock_guard<mutex_type> lock_type;
			typedef typename shared_mem<Ch>::proj_id_type proj_id_type;

			explicit basic_shmembuf(id_type id):
				_sharedmem(id, 512, BUFFER_PROJID),
				_sharedpart(new (this->_sharedmem.ptr()) shmem_header)
			{
				this->_sharedpart->size = this->_sharedmem.size();
				char_type* ptr = this->_sharedmem.begin() + sizeof(shmem_header);
				this->setg(ptr, ptr, ptr);
				this->setp(ptr, this->_sharedmem.end());
				this->debug("basic_shmembuf()");
			}

			basic_shmembuf(id_type id, int):
				_sharedmem(id, BUFFER_PROJID),
				_sharedpart(reinterpret_cast<shmem_header*>(this->_sharedmem.ptr()))
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

			void create(id_type id) {
				this->_sharedmem.create(id, 512, BUFFER_PROJID);
				this->_sharedpart = new (this->_sharedmem.ptr()) shmem_header;
				this->_sharedpart->size = this->_sharedmem.size();
				char_type* ptr = this->_sharedmem.begin() + sizeof(shmem_header);
				this->setg(ptr, ptr, ptr);
				this->setp(ptr, this->_sharedmem.end());
				this->writeoffs();
				this->debug("basic_shmembuf::create()");
			}
			
			void attach(id_type id) {
				this->_sharedmem.attach(id, BUFFER_PROJID),
//				this->_sharedpart = new (this->_sharedmem.ptr()) shmem_header;
				this->_sharedpart = reinterpret_cast<shmem_header*>(this->_sharedmem.ptr());
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
				Logger<Level::SHMEM>()
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
					throw Error("bad shared_mem size",
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

			shared_mem<char_type> _sharedmem;
			struct shmem_header {
				size_type size = 0;
				mutex_type mtx;
				pos_type goff = 0;
				pos_type poff = 0;
			} *_sharedpart = nullptr;

			static const proj_id_type BUFFER_PROJID = 'b';
		};

	}

	using components::Process;
	using components::Process_group;

}
