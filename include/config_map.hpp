#ifndef CARL_CONFIG_MAP_HPP
#define CARL_CONFIG_MAP_HPP

#include "iconfig_value.hpp"
#include "utils/exceptions.hpp"

#include <fmt/format.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

namespace CARL
{

enum class MapType : uint8_t { ID_LIST, STANDARD, };

/// @brief Wrapper for Config Maps. Can parse usual Map, or a List containing ID members
template <typename Group, typename KeyType = int>
class ConfigMap : public IConfigValue
{
    using Storage = std::map<KeyType, std::unique_ptr<Group>>;

    /// @brief keeps the owning pointer out of the public API, so iteration yields (key, group) references
    template <typename StorageIterator, typename GroupType>
    class Iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = std::pair<KeyType const&, GroupType&>;
        using difference_type   = std::ptrdiff_t;
        using reference         = value_type;
        using pointer           = void;

        explicit Iterator(StorageIterator it)
            : it_(it) {}

        value_type operator *() const { return value_type {it_->first, *it_->second}; }

        Iterator& operator ++()    { ++it_; return *this; }
        Iterator  operator ++(int) { Iterator previous {*this}; ++it_; return previous; }

        bool operator ==(Iterator const& other) const { return it_ == other.it_; }
        bool operator !=(Iterator const& other) const { return it_ != other.it_; }

    private:
        StorageIterator it_;
    };


public:
    static_assert(std::is_default_constructible_v<Group>, "ConfigMap<Group>: Group must be default-constructible");
    static_assert(
        fmt::is_formattable<KeyType>::value,
        "ConfigMap<Group, KeyType>: KeyType must be formattable by fmt, it appears in keys and error messages"
    );

    using key_type       = KeyType;
    using mapped_type    = Group;
    using iterator       = Iterator<typename Storage::iterator, Group>;
    using const_iterator = Iterator<typename Storage::const_iterator, Group const>;


public:
    explicit ConfigMap(std::string name, MapType type = MapType::STANDARD, Required required = Required::YES)
        : name_(std::move(name)),
          isRequired_(required),
          mapType_(type) {}

    void                           parse(YAML::Node const& node) override;
    [[nodiscard]] ValidationResult validate() const override;
    void                           printTo(std::ostream& os, std::string const& indent = "") const override;
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

    /// @brief the entry stored under key
    /// @throws LookupError when no entry is keyed by key
    [[nodiscard]] Group&       at(KeyType const& key);
    [[nodiscard]] Group const& at(KeyType const& key) const;

    /// @brief the entry stored under key, or nullptr when there is none
    [[nodiscard]] Group*       find(KeyType const& key);
    [[nodiscard]] Group const* find(KeyType const& key) const;

    [[nodiscard]] bool        contains(KeyType const& key) const { return entries_.count(key) > 0; }
    [[nodiscard]] std::size_t size() const noexcept              { return entries_.size(); }
    [[nodiscard]] bool        empty() const noexcept             { return entries_.empty(); }

    /// @brief iteration yields std::pair<KeyType const&, Group&>, so bind it by value or by const reference
    [[nodiscard]] iterator       begin() noexcept        { return iterator {entries_.begin()}; }
    [[nodiscard]] iterator       end() noexcept          { return iterator {entries_.end()}; }
    [[nodiscard]] const_iterator begin() const noexcept  { return const_iterator {entries_.begin()}; }
    [[nodiscard]] const_iterator end() const noexcept    { return const_iterator {entries_.end()}; }
    [[nodiscard]] const_iterator cbegin() const noexcept { return begin(); }
    [[nodiscard]] const_iterator cend() const noexcept   { return end(); }


private:
    std::string name_;
    Required    isRequired_;
    MapType     mapType_;

    Storage entries_;
    bool    wasParsed_ {false};

    std::string niceName() const { return name_.empty()? "<nameless map>" : name_; }

    KeyType parseKey(YAML::Node const& key_node) const;
};

} // namespace CARL


#include "config_map.inl"

#endif // CARL_CONFIG_MAP_HPP
