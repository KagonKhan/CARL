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

`yaml-cpp` and `fmt` are found with `find_package`, so a plain `cmake ..` only works if both are
already installed where CMake can see them (a system package manager, or your own
`CMAKE_PREFIX_PATH`). Otherwise use the Conan flow above.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/deps
cmake --build build
ctest --test-dir build
```

Tests default to on for a top-level build and off when CARL is pulled in with
`add_subdirectory`. Force either way with `-DCARL_BUILD_TESTS=ON|OFF`.

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

`patch()` sets a value from code rather than from YAML. A patched field counts as set — it satisfies
`validate()` and prints with a `(patched)` tag — so it is a way to fill a required field the config
file cannot supply.

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
    CARL::ConfigValue<int>         id    {"id"};
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

> The key is read from the YAML `id` field, so the group must declare a matching `id` member for the
> value to be readable afterwards. Registration order only affects print order.

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

#### Reading the entries

`ConfigMap` reads like a `std::map`, except it hands out `Group&` instead of the owning pointer it
stores internally.

```cpp
CameraEntry&       front = cfg.cameras.at(1);        // throws CARL::LookupError if absent
CameraEntry const* maybe = cfg.cameras.find(2);      // nullptr if absent

if (cfg.cameras.contains(3)) { /* ... */ }
std::size_t how_many = cfg.cameras.size();
bool        none     = cfg.cameras.empty();
```

Iteration yields `std::pair<KeyType const&, Group&>` in key order. It is a proxy pair, so bind it by
value or by const reference — never by non-const reference:

```cpp
for (auto const& [id, camera] : cfg.cameras) {
    std::cout << id << ": " << *camera.model << " @ " << *camera.zoom << "\n";
}
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

Later parses overlay earlier ones all the way down the tree. For a `ConfigMap` that means an entry
whose key already exists is merged into rather than replaced, so a second file can override single
fields of an existing entry and introduce new entries at the same time. Duplicate keys are rejected
**within one document**, not across files.

Unknown YAML fields are silently ignored. Missing required fields produce entries in
`ValidationResult::errors` with fully-qualified names (`cameras[1].model: is missing`).

A required group or map that is absent altogether reports itself once rather than reporting each of
its fields:

```
database: is missing
'cameras' is required and missing
```

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

## Generating a model from YAML

`tools/carl_generate.py` turns one complete config file into a CARL model. It needs Python 3 and
PyYAML.

```bash
python3 tools/carl_generate.py config.yaml -o include/generated -n myapp -r AppConfig -b myapp
```

It writes `<basename>_config.hpp` (the tree of `ConfigGroup`, `ConfigMap` and `ConfigValue`) and
`<basename>_extensions.hpp` (plain types for shapes CARL has no group for, with their
`YAML::convert` and `operator<<`).

Structure comes from the YAML itself:

| YAML | Generated |
|------|-----------|
| mapping of scalars | `ConfigGroup` with one `ConfigValue` per key |
| mapping whose values are similar mappings | `ConfigMap<Entry, KeyType>`, `MapType::STANDARD` |
| sequence of mappings that all have `id` | `ConfigMap<Entry>`, `MapType::ID_LIST` |
| sequence of mappings without `id` | generated struct + `ConfigValue<std::vector<Struct>>` |
| sequence of scalars or of sequences | generated wrapper struct |

Scalar types widen: a key seen as a float becomes `double`, integers become `int`, `true`/`false`
becomes `bool`, anything else stays `std::string`.

### Annotations

One file cannot show which keys are optional, so declare it inline. Directives are written with a
leading `!` and are read from a trailing comment on the key's line, or from a standalone comment line
directly above it — prose comments are never mistaken for directives.

```yaml
video:
  hw: cpu              # !default    -> Default<std::string>{"cpu"}
  maxEncoders: 8       # !optional   -> Required::NO
bladeRecognizer:       # !optional   -> optional group
cameras:               # !map        -> force a keyed ConfigMap
recognizers:           # !group      -> force a group of nested groups
stations:              # !list       -> keep a sequence out of ID_LIST mode
```

A YAML tag names the C++ type directly. Tags naming your own type require you to supply
`YAML::convert<T>` and `operator<<`; the generated header `static_assert`s on
`is_carl_parseable<T>` so a missing one is a readable compile error. Pass `--tag-include` to have
your header included above that assert.

```yaml
        transformationMatrix: !cv::Mat   # ConfigValue<cv::Mat>, you provide convert + operator<<
        radius: !double 2                # a builtin tag is just a type override
```

Within a `ConfigMap`, keys present in only some entries are inferred optional automatically. The
generator prints a note for every such inference, and for every shape it had to push into a
generated struct — those lose per-field validation, so a bad value reports against the whole field
rather than the exact path.

---

## Error handling

| Situation | Behaviour |
|-----------|-----------|
| YAML is structurally wrong (sequence where map expected) | throws `CARL::ParsingError` |
| Scalar where a group or map was expected (`server: hello`) | throws `CARL::ParsingError` |
| Field type mismatch (`x: notanumber` for `ConfigValue<int>`) | throws `CARL::ParsingError` |
| Required field absent | `validate()` returns failure with field name in errors |
| Required group or map absent | `validate()` returns one failure naming the section |
| Duplicate `id` within one `ID_LIST` document | throws `CARL::ParsingError` |
| `ConfigMap::at()` with an unknown key | throws `CARL::LookupError` |
| Accessing unset value | `assert` in debug builds; always validate before accessing |

Every exception CARL throws derives from `CARL::FormattedException`, and therefore from
`std::runtime_error`. `parse()` does not let raw `YAML::Exception` escape.
