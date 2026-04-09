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
        return ValidationResult::failure("{}: is missing", name_);
    }

    return ValidationResult::success();
}

template <typename T>
void ConfigValue<T>::printTo(std::ostream& os, std::string const& indent) const
{
    if (value_) {
        std::ostringstream valueStream;
        valueStream << *value_;
        std::string valueStr = valueStream.str();

        // re-indent every line of the value
        std::string        indentedValue;
        std::istringstream lines(valueStr);
        std::string        line;
        bool               first = true;
        while (std::getline(lines, line)) {
            if (!first) {
                indentedValue += "\n" + indent + "  ";
            }

            indentedValue += line;
            first          = false;
        }

        os << indent << fmt::format("{}: {} {}\n", name_, indentedValue, wasParsed_? "" : "(default)");
    }
    else {
        os << indent << fmt::format("{}: {}\n", name_, "<not set>");
    }
}

} // namespace CARL
