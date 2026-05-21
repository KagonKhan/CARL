# CARL

**C**onfiguration **A**nd **R**epresentation **L**ibrary — a C++17 library for parsing YAML configuration files into strongly-typed structures.

## Overview

CARL maps YAML onto a hierarchy of typed C++ objects. You define the shape of your config as a tree of `ConfigValue`, `ConfigGroup`, and `ConfigMap` members, call `parse()`, then `validate()`. Access is type-safe, required fields are enforced, and the whole tree can be printed for debugging.

```cpp
struct AppConfig : CARL::ConfigGroup {
    CARL::ConfigValue<std::string> host {"host"};
    CARL::ConfigValue<int>         port {"port"};
    CARL::ConfigValue<int>         timeout {"timeout", CARL::Default<int>{30}};

    AppConfig() : CARL::ConfigGroup("server") {
        registerEntries(host, port, timeout);
    }
};

AppConfig cfg;
cfg.parse(YAML::LoadFile("config.yaml"));

auto result = cfg.validate();
if (!result.correct) {
    for (auto const& err : result.errors) std::cerr << err << "\n";
    return 1;
}

std::cout << *cfg.host << ":" << *cfg.port << "\n";
```

```yaml
# config.yaml
server:
  host: localhost
  port: 8080
```

## Building

CARL uses [Conan 2](https://conan.io/) for dependencies and CMake 3.23+.

```bash
conan install . --build=missing
cmake --preset conan-release
cmake --build --preset conan-release
ctest --preset conan-release
```

Or manually:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
ctest
```

### Dependencies

| Library  | Version | Purpose                  |
|----------|---------|--------------------------|
| yaml-cpp | 0.9.0   | YAML parsing             |
| fmt      | 12.1.0  | Error message formatting |
| gtest    | 1.14.0  | Tests (optional)         |

### CMake integration

After installing, link against `carl::carl`:

```cmake
find_package(carl REQUIRED)
target_link_libraries(my_target PRIVATE carl::carl)
```

---

## Core concepts

### `ConfigValue<T>` — a single typed field

Wraps one value of type `T`. Requires `T` to be printable and YAML-parsable (see [Custom types](#custom-types)).

```cpp
CARL::ConfigValue<int>         port    {"port"};                              // required
CARL::ConfigValue<int>         retries {"retries", CARL::Required::NO};       // optional, no default
CARL::ConfigValue<std::string> mode    {"mode", CARL::Default<std::string>{"fast"}}; // optional with default
```

Access the value after validation via `operator*`, `operator->`, or `.value()`:

```cpp
int p        = *cfg.port;
std::string m = cfg.mode.value();
size_t len    = cfg.name->size();   // operator-> for member access
```

Accessing an unset value triggers an assertion in debug builds. Always validate first.

### `ConfigGroup` — a named section

Groups a set of fields or nested groups under a YAML key. Subclass it, declare members, then register them in the constructor.

```cpp
struct DatabaseConfig : CARL::ConfigGroup {
    CARL::ConfigValue<std::string> host {"host"};
    CARL::ConfigValue<int>         port {"port"};

    DatabaseConfig() : CARL::ConfigGroup("database") {
        registerEntries(host, port);
    }
};
```

```yaml
database:
  host: db.internal
  port: 5432
```

#### Named vs nameless groups

Whether to pass a name to `ConfigGroup` is the one thing to get right.

- **Named group** (`ConfigGroup("database")`) — parsed from `node["database"]`. Use for top-level sections and any nested group that corresponds to a distinct YAML key.
- **Nameless group** (`ConfigGroup("")`) — parsed from the current node directly. Use for the root config object, or for entries inside a `ConfigMap` (where the map already positions the node).

```cpp
// Root config — nameless, parsed from the document root
struct RootConfig : CARL::ConfigGroup {
    DatabaseConfig db;
    ServerConfig   server;

    RootConfig() : CARL::ConfigGroup("") {
        registerEntries(db, server);
    }
};
```

#### Optional groups

A group constructed with `Required::NO` validates successfully even if its YAML key is absent:

```cpp
struct DebugSection : CARL::ConfigGroup {
    CARL::ConfigValue<int> verbosity {"verbosity"};
    DebugSection() : CARL::ConfigGroup("debug", CARL::Required::NO) {
        registerEntries(verbosity);
    }
};
```

### `ConfigMap<Group, KeyType>` — a keyed collection

Wraps a `std::map<KeyType, std::unique_ptr<Group>>`. Two modes:

#### `MapType::ID_LIST` — sequence of objects identified by an `id` field

Each YAML entry is a mapping; the `id` field value becomes the map key.

```cpp
struct CameraEntry : CARL::ConfigGroup {
    CARL::ConfigValue<int>         id    {"id"};    // MUST be registered first
    CARL::ConfigValue<std::string> model {"model"};
    CARL::ConfigValue<int>         zoom  {"zoom"};

    CameraEntry() { registerEntries(id, model, zoom); } // nameless — map positions the node
};

CARL::ConfigMap<CameraEntry> cameras {"cameras", CARL::MapType::ID_LIST};
```

```yaml
cameras:
  - id: 1
    model: AXIS-P3245
    zoom: 10
  - id: 2
    model: AXIS-Q6135
    zoom: 30
```

> The `id` field **must be the first registered entry** in an ID_LIST group.

#### `MapType::STANDARD` — map keyed by YAML key names

```cpp
CARL::ConfigMap<CameraEntry, std::string> cameras {"cameras"};
```

```yaml
cameras:
  front:
    model: AXIS-P3245
    zoom: 10
  rear:
    model: AXIS-Q6135
    zoom: 30
```

---

## Parsing and validation

```cpp
// Single file
cfg.parse(YAML::LoadFile("config.yaml"));

// Multi-file: later parses overwrite only the fields they contain
cfg.parse(YAML::LoadFile("base.yaml"));
cfg.parse(YAML::LoadFile("override.yaml"));

// Validate the whole tree at once
auto result = cfg.validate();
if (!result.correct) {
    for (auto const& err : result.errors)
        std::cerr << err << "\n";
}
```

Unknown YAML fields are silently ignored. Missing required fields produce entries in `ValidationResult::errors` with fully-qualified names (`cameras[1].model: is missing`).

---

## Debug printing

```cpp
cfg.printTo(std::cout, "");
```

Prints the entire config tree with source annotations:

```
server:
  host: localhost
  port: 8080
  timeout: 30 (default)
  api_key: <missing>
```

---

## Custom types

Any type `T` can be used in `ConfigValue<T>` by providing exactly two things. CARL enforces both at compile time via `static_assert`.

### 1. YAML decoding — specialize `YAML::convert<T>`

```cpp
namespace YAML {
    template <>
    struct convert<MyType> {
        static bool decode(Node const& node, MyType& out) {
            out.x = node["x"].as<int>();
            return true;
        }
    };
}
```

### 2. Printing — overload `operator<<`

```cpp
inline std::ostream& operator<<(std::ostream& os, MyType const& v) {
    return os << v.x;
}
```

Multi-line output is supported and automatically reindented by `printTo()`.

### Complete example

```cpp
struct Color {
    uint8_t r {0}, g {0}, b {0};
};

inline std::ostream& operator<<(std::ostream& os, Color const& c) {
    return os << "r: " << (int)c.r
              << "\ng: " << (int)c.g
              << "\nb: " << (int)c.b;
}

namespace YAML {
    template <>
    struct convert<Color> {
        static bool decode(Node const& node, Color& c) {
            c.r = node["r"].as<uint8_t>();
            c.g = node["g"].as<uint8_t>();
            c.b = node["b"].as<uint8_t>();
            return true;
        }
    };
}

// Now usable anywhere in a config tree:
CARL::ConfigValue<Color> led {"led", CARL::Default<Color>{Color{0, 255, 0}}};
```

```yaml
led:
  r: 255
  g: 128
  b: 0
```

### Compile-time verification

You can assert that a type satisfies CARL's requirements before using it:

```cpp
#include "utils/constraints.hpp"

static_assert(CARL::is_carl_parseable<Color>, "Color needs YAML::convert<> and operator<<");
```

---

## Error handling

| Situation | Behaviour |
|-----------|-----------|
| YAML is structurally wrong (sequence where map expected) | throws `CARL::ParsingError` |
| Field type mismatch (`x: notanumber` for `ConfigValue<int>`) | throws `CARL::ParsingError` |
| Required field absent | `validate()` returns failure with field name in errors |
| Duplicate `id` in `ID_LIST` map | throws `CARL::ParsingError` |
| Accessing unset value | `assert` in debug builds; always validate before accessing |
