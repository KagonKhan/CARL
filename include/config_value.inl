#include "utils/utils.hpp"

namespace CARL
{

template <typename T>
ConfigValue<T>::ConfigValue(std::string name, Required required)
    : name_(std::move(name)),
      required_(required),
      value_(std::nullopt),
      source_(ValueSource::unset()) {}

template <typename T>
ConfigValue<T>::ConfigValue(std::string name, Default<T> default_value)
    : name_(std::move(name)),
      required_(Required::NO),
      value_(std::move(default_value.value)),
      source_(ValueSource::fromDefault()) {}

template <typename T>
void ConfigValue<T>::parse(YAML::Node const& node)
{
    if (!node.IsMap()) {
        throw ParsingError("expected a map node while parsing field '{}'", name_);
    }

    if (auto field = node[name_]) {
        try {
            value_ = field.as<T>();
            source_.markParsed();
        }
        catch (YAML::Exception const& e) {
            throw ParsingError("Could not parse {}, with: {}", name_, e.what());
        }
    }
}

template <typename T>
ValidationResult ConfigValue<T>::validate() const
{
    if (required_ && !source_.isSet()) {
        return ValidationResult::failure("{}: is missing", name_);
    }
    else {
        return ValidationResult::success();
    }
}

template <typename T>
void ConfigValue<T>::printTo(std::ostream& os, std::string const& indent) const
{
    std::ostringstream ss;
    if (value_) {
        ss << *value_;
    }

    os << indent << fmt::format("{}: {} {}\n", name_, reindent(ss.str(), indent), source_.displayTag());
}

} // namespace CARL
