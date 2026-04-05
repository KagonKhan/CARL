namespace CARL
{

template <typename T>
constexpr ConfigValue<T>::ConfigValue(std::string name, Required required)
    : name_(std::move(name)),
      required_(required) {}

template <typename T>
constexpr ConfigValue<T>::ConfigValue(std::string name, Default<T> default_value)
    : name_(std::move(name)),
      required_(Required::NO),
      value_(std::move(default_value.value)) {}

template <typename T>
void ConfigValue<T>::parse(YAML::Node const& node)
{
    if (!node.IsMap()) {
        throw ParsingError("expected a map node while parsing field '{}'", name_);
    }

    if (auto field = node[name_]) {
        try {
            value_     = field.as<T>();
            wasParsed_ = true;
        }
        catch (YAML::Exception const& e) {
            throw ParsingError("Could not parse {}, with: {}", name_, e.what());
        }
    }
}

template <typename T>
ValidationResult ConfigValue<T>::validate() const
{
    if ((required_ == Required::YES) && !value_.has_value()) {
        return ValidationResult::failure(fmt::format("{}: is missing", name_));
    }

    return ValidationResult::success();
}

template <typename T>
void ConfigValue<T>::printTo(std::ostream& os, std::string_view indent) const
{
    if (value_) {
        os << indent << fmt::format("{}: {} {}\n", name_, *value_, wasParsed_? "" : "(default)");
    }
    else {
        os << indent << fmt::format("{}: {}\n", name_, "<not set>");
    }
}

} // namespace CARL
