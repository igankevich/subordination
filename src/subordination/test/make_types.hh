#ifndef SUBORDINATION_TEST_MAKE_TYPES_HH
#define SUBORDINATION_TEST_MAKE_TYPES_HH

template <class A, class B>
struct TypePair {
    typedef A First;
    typedef B Second;
};

#define MAKE_TYPES(...) ::testing::Types<__VA_ARGS__>

#define MAKE_TYPE_PAIR(a, b) ::TypePair<a,b>

#endif // vim:filetype=cpp
