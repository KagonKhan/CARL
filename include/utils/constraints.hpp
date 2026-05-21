#ifndef CARL_CONSTRAINTS_HPP
#define CARL_CONSTRAINTS_HPP

#include <yaml-cpp/yaml.h>

#include <ostream>
#include <type_traits>
#include <utility>

namespace CARL
{

template <typename T, typename = void>
struct is_printable : std::false_type {};

template <typename T>
struct is_printable<
    T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>
> : std::true_type {};

template <typename T, typename = void>
struct is_yaml_parsable : std::false_type {};

template <typename T>
struct is_yaml_parsable<
    T, std::void_t<decltype(YAML::convert<T>::decode(
        std::declval<const YAML::Node&>(),
        std::declval<T&>()))>
> : std::true_type {};

// Users can add this in their extension headers to validate early:
// static_assert(CARL::is_carl_parseable<MyType>, "MyType needs YAML::convert<> and operator<<");
template <typename T>
constexpr bool is_carl_parseable = is_yaml_parsable<T>::value && is_printable<T>::value;

// Concept when they become available
// template<typename T>
// concept ConfigParseable = requires(YAML::Node n, std::ostream os, const T& val) {
//     { n.as<T>() };
//     { os << val };
// };

} // namespace CARL

#endif // CARL_CONSTRAINTS_HPP
