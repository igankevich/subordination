#ifndef TEST_MAKE_TYPES_HH
#define TEST_MAKE_TYPES_HH

template <class A, class B>
struct TypePair {
	typedef A First;
	typedef B Second;
};

#define MAKE_TYPES(...) ::testing::Types<__VA_ARGS__>

#define MAKE_TYPE_PAIR(a, b) ::TypePair<a,b>

#endif // TEST_MAKE_TYPES_HH vim:filetype=cpp

