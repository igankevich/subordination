namespace factory {

	namespace components {

		template<class K, class V>
		struct Repository {

			Repository(): _values() {}

			V get(K key) const {
				auto result = _values.find(key);
				return result == _values.end() ? nullptr : result->second; 
			}

			void put(K key, V val) { _values[key] = val; }
		
			friend std::ostream& operator<<(std::ostream& out, const Repository& rhs) {
				std::ostream_iterator<Entry> it(out, "\n");
				std::copy(rhs._values.cbegin(), rhs._values.cend(), it);
				return out;
			}

		private:

			struct Entry {
				Entry(const std::pair<const K, V>& k): _key(k.first), _val(k.second) {}

				friend std::ostream& operator<<(std::ostream& out, const Entry& rhs) {
					return out
						<< "/repo/"
						<< rhs._key
						<< " = "
						<< rhs._val;
				}

			private:
				K _key;
				V _val;
			};

			std::unordered_map<K, V> _values;
		};

		template<template<class X, class Y> class Repo, class K=Null, class V=Null, class ... Next_repo>
		struct Repository_stack: public Repo<K, V>, public Repository_stack<Repo, Next_repo...> {

			typedef Repository_stack<Repo, K, V, Next_repo...> This;
			typedef Repository_stack<Repo, Next_repo...> Next;
			typedef Repo<K,V> R;

			using R::put;
			using R::get;

			using Next::put;
			using Next::get;
//
//			void put(K key, V val) { Repo<K,V>::put(key, val); }
//			V get(K key) const { return Repo<K,V>::get(key); }

			friend std::ostream& operator<<(std::ostream& out, const This& rhs) {
				return out
					<< static_cast<const R&>(rhs)
					<< static_cast<const Next&>(rhs);
			}

		};

		template<template<class X, class Y> class Repo>
		struct Repository_stack<Repo, Null, Null> {
			typedef Repository_stack<Repo, Null, Null> This;
			void put() {}
			void get() {}
			friend std::ostream& operator<<(std::ostream& out, const This&) { return out; }
		};

	}

}
