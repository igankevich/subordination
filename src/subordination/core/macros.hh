#ifndef SUBORDINATION_CORE_MACROS_HH
#define SUBORDINATION_CORE_MACROS_HH

#include <type_traits>

#define SBN_FLAGS(ENUM) \
    constexpr inline ENUM operator|(ENUM a, ENUM b) noexcept { \
        using type = typename ::std::underlying_type<ENUM>::type; \
        return ENUM(type(a) | type(b)); \
    } \
    constexpr inline ENUM operator&(ENUM a, ENUM b) noexcept { \
        using type = typename ::std::underlying_type<ENUM>::type; \
        return ENUM(type(a) & type(b)); \
    } \
    constexpr inline ENUM operator^(ENUM a, ENUM b) noexcept { \
        using type = typename ::std::underlying_type<ENUM>::type; \
        return ENUM(type(a) ^ type(b)); \
    } \
    constexpr inline ENUM operator~(ENUM a) noexcept { \
        using type = typename ::std::underlying_type<ENUM>::type; \
        return ENUM(~type(a)); \
    } \
    constexpr inline ENUM operator!(ENUM a) noexcept { \
        using type = typename ::std::underlying_type<ENUM>::type; \
        return ENUM(!type(a)); \
    } \
    inline ENUM& operator|=(ENUM& a, ENUM b) noexcept { \
        using type = typename ::std::underlying_type<ENUM>::type; \
        return a = ENUM(type(a) | type(b)); \
    } \
    inline ENUM& operator&=(ENUM& a, ENUM b) noexcept { \
        using type = typename ::std::underlying_type<ENUM>::type; \
        return a = ENUM(type(a) & type(b)); \
    } \
    inline ENUM& operator^=(ENUM& a, ENUM b) noexcept { \
        using type = typename ::std::underlying_type<ENUM>::type; \
        return a = ENUM(type(a) ^ type(b)); \
    }

#endif // vim:filetype=cpp
