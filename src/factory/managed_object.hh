#ifndef FACTORY_MANAGED_OBJECT_HH
#define FACTORY_MANAGED_OBJECT_HH

#include <unordered_set>

namespace factory {

	namespace components {

		struct Category {

			typedef std::function<void*()> construct_type;
			typedef std::string name_type;
			typedef std::string value_type;
			typedef std::string key_type;
			typedef std::vector<key_type> params_type;
			typedef std::function<value_type(const void*,key_type)> getparam_type;

			name_type _name;
			construct_type _construct;
			params_type _params;
			getparam_type _getparam;

			friend std::ostream&
			operator<<(std::ostream& out, const Category& rhs) {
				return out << rhs._name;
			}

		};

		template<class T>
		struct Managed_object: public T {

			typedef std::string name_type;
			typedef std::function<void(const Managed_object<T>*)> for_each_func_type;

			void
			setname(name_type&& rhs) {
				_name = std::forward<name_type>(rhs);
			}

			const name_type&
			name() const noexcept {
				return _name;
			}

			virtual void
			setparent(Managed_object* rhs) {
				T::setparent(rhs);
				if (rhs != this) {
					if (_parent) {
						_parent->remove_child(this);
					}
					_parent = rhs;
					if (rhs) {
						rhs->add_child(this);
					}
				}
			}

			inline Managed_object*
			parent() const noexcept {
				return _parent;
			}

			inline Managed_object*
			root() noexcept {
				return _parent
					? _parent->root()
					: this;
			}

			inline const Managed_object*
			root() const noexcept {
				return _parent ? _parent->root() : this;
			}

			virtual void add_child(Managed_object*) {}
			virtual void remove_child(Managed_object*) {}

			virtual void
			for_each(for_each_func_type) const {}

			template<class Foreign>
			void
			write_foreign(const Foreign& obj, name_type&& name, std::ostream& out) const {
				obj.write_recursively(out,
					[this,&name] (std::ostream& out) {
						this->write_prefix(out);
						out << KEY_SEPARATOR << name << KEY_SEPARATOR;
					}
				);
			}

			void
			write_prefix(std::ostream& out) const {
				write_categories(out);
			}

			virtual void
			write_recursively(std::ostream& out) const {
				write_prefix(out);
				out << LINE_SEPARATOR;
				write_configuration(out);
				for_each([&out] (const Managed_object* rhs) { rhs->write_recursively(out); });
			}

			void
			write_categories(std::ostream& out) const {
				if (_parent) {
					_parent->write_categories(out);
				}
				out << KEY_SEPARATOR << _name;
			}

			void
			write_configuration(std::ostream& out) const {
				Category cat = category();
				std::for_each(cat._params.begin(), cat._params.end(),
					[this,&out,&cat] (const Category::key_type& key) {
						write_prefix(out);
						out << KEY_SEPARATOR << key
							<< KEY_VALUE_SEPARATOR
							<< cat._getparam(this, key)
							<< LINE_SEPARATOR;
					}
				);
			}

			void
			dump_hierarchy(std::ostream& out) {
				out << *root();
			}

			friend std::ostream&
			operator<<(std::ostream& out, const Managed_object& rhs) {
				rhs.write_recursively(out); return out;
			}

			virtual Category
			category() const noexcept = 0;

		private:
			name_type _name{};
			Managed_object* _parent = nullptr;

			static constexpr const char
			KEY_SEPARATOR = '/';
			static constexpr const char
			KEY_VALUE_SEPARATOR = '=';
			static constexpr const char
			LINE_SEPARATOR = '\n';
		};

		template<class T>
		struct Managed_set: public Managed_object<T> {

			void
			add_child(Managed_object<T>* rhs) override {
				_children.insert(rhs);
			}

			void
			remove_child(Managed_object<T>* rhs) override {
				_children.erase(rhs);
			}

			void
			for_each(std::function<void(const Managed_object<T>*)> func) const override {
				std::for_each(_children.begin(), _children.end(), func);
			}

		private:
			std::unordered_set<Managed_object<T>*> _children;
		};
	
	}

}
#endif // FACTORY_MANAGED_OBJECT_HH
