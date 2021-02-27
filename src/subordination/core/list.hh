#ifndef SUBORDINATION_CORE_LIST_HH
#define SUBORDINATION_CORE_LIST_HH

#include <ostream>
#include <sstream>

namespace sbn {

    template <class ... Elements>
    class List;

    template <>
    class List<> {
    public:
        static constexpr inline size_t size() noexcept { return 0; }
        inline void write(std::ostream&) const {}
    };

    template <class Head, class ... Tail>
    class List<Head,Tail...> {

    public:
        using value_type = Head;
        using head_type = Head;
        using tail_type = List<Tail...>;

    private:
        head_type _head;
        tail_type _tail;

    public:
        inline List(Head&& head, Tail&& ... tail):
        _head{std::forward<Head>(head)}, _tail{std::forward<Tail>(tail)...} {}
        //inline List(const Head& head, const Tail& ... tail):
        //_head{head}, _tail{tail...} {}
        const Head& head() const noexcept { return this->_head; }
        Head& head() noexcept { return this->_head; }
        const List<Tail...>& tail() const noexcept { return this->_tail; }
        List<Tail...>& tail() noexcept { return this->_tail; }
        static constexpr inline size_t size() noexcept { return 1+sizeof...(Tail); }
        inline void write(std::ostream& out) const {
            out << ' ' << head();
            tail().write(out);
        }
        inline void write_first(std::ostream& out) const {
            out << head();
            tail().write(out);
        }
        List() = default;
        ~List() = default;
        List(const List&) = default;
        List& operator=(const List&) = default;
        List(List&&) = default;
        List& operator=(List&&) = default;
    };

    template <class ... Elements>
    inline List<Elements...> list(Elements&& ... elems) noexcept {
        return List<Elements...>(std::forward<Elements>(elems)...);
    }

    inline std::ostream&
    operator<<(std::ostream& out, const List<>& rhs) {
        return out << "()";
    }

    template <class ... Elements> inline std::ostream&
    operator<<(std::ostream& out, const List<Elements...>& rhs) {
        out << '(';
        rhs.write_first(out);
        out << ')';
        return out;
    }

    template <class Container>
    class list_view {
    private:
        const Container& _elements;
    public:
        inline explicit list_view(const Container& elements) noexcept: _elements(elements) {}
        inline void write(std::ostream& out) const {
            bool first = true;
            for (const auto& elem : this->_elements) {
                if (!first) { out << ' '; }
                out << elem;
                first = false;
            }
        }
        list_view() = default;
        ~list_view() = default;
        list_view(const list_view&) = default;
        list_view& operator=(const list_view&) = default;
        list_view(list_view&&) = default;
        list_view& operator=(list_view&&) = default;
    };

    template <class Container> inline std::ostream&
    operator<<(std::ostream& out, const list_view<Container>& rhs) {
        out << '(';
        rhs.write(out);
        out << ')';
        return out;
    }

    template <class Container> inline list_view<Container>
    make_list_view(const Container& c) noexcept { return list_view<Container>(c); }

    template <class Container>
    class text {
    private:
        const Container& _elements;
    public:
        inline explicit text(const Container& elements) noexcept: _elements(elements) {}
        inline void write(std::ostream& out) const {
            std::stringstream tmp;
            tmp << this->_elements;
            auto str = tmp.str();
            for (auto ch : str) {
                if (ch == '\\' || ch == '"') {
                    out << '\\';
                }
                out << ch;
            }
        }
        text() = default;
        ~text() = default;
        text(const text&) = default;
        text& operator=(const text&) = default;
        text(text&&) = default;
        text& operator=(text&&) = default;
    };

    template <class Container> inline std::ostream&
    operator<<(std::ostream& out, const text<Container>& rhs) {
        out << '"';
        rhs.write(out);
        out << '"';
        return out;
    }

    template <class Container> inline text<Container>
    make_string(const Container& c) noexcept { return text<Container>(c); }

}

namespace std {
    template <class A, class B> inline ostream&
    operator<<(ostream& out, const pair<A,B>& rhs) {
        return out << sbn::list(rhs.first, rhs.second);
    }
}

#endif // vim:filetype=cpp
