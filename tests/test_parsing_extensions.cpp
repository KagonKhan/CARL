// Demonstrates extending CARL with user-defined types.
//
// The library requires exactly two things from any type T used in ConfigValue<T>:
//   1. A YAML::convert<T> specialization with a static decode() method
//   2. An operator<<(std::ostream&, T const&) overload
//
// Nothing else needs to be registered with CARL itself.
// The static_asserts on ConfigValue enforce these requirements at compile time.

#include "config_group.hpp"
#include "config_map.hpp"
#include "config_value.hpp"
#include "utils/constraints.hpp"

#include <gtest/gtest.h>
#include <cstdint>
#include <sstream>

// ============================================================
//  Custom type 1: Color — simple scalar-like struct
//
//  YAML representation:
//    color:
//      r: 255
//      g: 128
//      b: 0
// ============================================================

struct Color
{
    uint8_t r {0};
    uint8_t g {0};
    uint8_t b {0};

    bool operator ==(Color const& o) const { return (r == o.r) && (g == o.g) && (b == o.b); }
};

inline std::ostream& operator <<(std::ostream& os, Color const& c)
{
    // Multi-line intentionally — exercises ConfigValue's reindent path
    return os << "r: " << static_cast<int>(c.r)
              << "\ng: " << static_cast<int>(c.g)
              << "\nb: " << static_cast<int>(c.b);
}

namespace YAML
{

template <>
struct convert<Color>
{
    static bool decode(Node const& node, Color& c)
    {
        c.r = node["r"].as<uint8_t>();
        c.g = node["g"].as<uint8_t>();
        c.b = node["b"].as<uint8_t>();
        return true;
    }
};

} // namespace YAML

// ============================================================
//  Custom type 2: GeoPoint — realistic domain type
//
//  YAML representation:
//    position:
//      lat: 52.2297
//      lon: 21.0122
//      alt: 110.5
// ============================================================

struct GeoPoint
{
    double lat {0.0};
    double lon {0.0};
    double alt {0.0};

    bool operator ==(GeoPoint const& o) const
    {
        return (lat == o.lat) && (lon == o.lon) && (alt == o.alt);
    }
};

inline std::ostream& operator <<(std::ostream& os, GeoPoint const& p)
{
    return os << "lat: " << p.lat
              << "\nlon: " << p.lon
              << "\nalt: " << p.alt;
}

namespace YAML
{

template <>
struct convert<GeoPoint>
{
    static bool decode(Node const& node, GeoPoint& p)
    {
        p.lat = node["lat"].as<double>();
        p.lon = node["lon"].as<double>();
        p.alt = node["alt"].as<double>();
        return true;
    }
};

} // namespace YAML

// Type with operator<< but no YAML::convert — must be at file scope so the
// trait check sees the same incomplete registration that a real user would have.
struct PrintOnlyType
{
    int x;
};

inline std::ostream& operator<<(std::ostream& os, PrintOnlyType const& p)
{
    return os << p.x;
}

// ============================================================
//  Trait verification — are the types recognised by CARL?
// ============================================================

TEST(CustomTypeTraits, ColorIsCarLParseable) {
    EXPECT_TRUE(CARL::is_carl_parseable<Color>);
    EXPECT_TRUE(CARL::is_printable<Color>::value);
    EXPECT_TRUE(CARL::is_yaml_parsable<Color>::value);
}

TEST(CustomTypeTraits, GeoPointIsCarLParseable) {
    EXPECT_TRUE(CARL::is_carl_parseable<GeoPoint>);
}

TEST(CustomTypeTraits, BuiltinTypesStillParseable) {
    EXPECT_TRUE(CARL::is_carl_parseable<int>);
    EXPECT_TRUE(CARL::is_carl_parseable<double>);
    EXPECT_TRUE(CARL::is_carl_parseable<std::string>);
    EXPECT_TRUE(CARL::is_carl_parseable<bool>);
}

TEST(CustomTypeTraits, TypeWithoutOverloadsIsNotParseable) {
    struct Unregistered { int x; };

    EXPECT_FALSE(CARL::is_carl_parseable<Unregistered>);
    EXPECT_FALSE(CARL::is_yaml_parsable<Unregistered>::value);
    EXPECT_FALSE(CARL::is_printable<Unregistered>::value);
}

TEST(CustomTypeTraits, TypeWithOnlyPrintIsNotFullyParseable) {
    // PrintOnlyType has operator<< but no YAML::convert<> — half-registered.
    // CARL requires both; is_carl_parseable must be false.
    EXPECT_TRUE (CARL::is_printable<PrintOnlyType>::value);
    EXPECT_FALSE(CARL::is_yaml_parsable<PrintOnlyType>::value);
    EXPECT_FALSE(CARL::is_carl_parseable<PrintOnlyType>);
}

// ============================================================
//  Color — ConfigValue<Color> parse
// ============================================================

TEST(ColorConfigValue, ParsesFromYaml) {
    CARL::ConfigValue<Color> cv {"color"};
    auto                     node = YAML::Load("color:\n  r: 255\n  g: 128\n  b: 0");
    cv.parse(node);
    EXPECT_TRUE(cv.validate().correct);
    EXPECT_EQ((*cv).r, 255u);
    EXPECT_EQ((*cv).g, 128u);
    EXPECT_EQ((*cv).b, 0u);
}

TEST(ColorConfigValue, ParsesBlack) {
    CARL::ConfigValue<Color> cv {"color"};
    auto                     node = YAML::Load("color:\n  r: 0\n  g: 0\n  b: 0");
    cv.parse(node);
    EXPECT_EQ((*cv).r, 0u);
    EXPECT_EQ((*cv).g, 0u);
    EXPECT_EQ((*cv).b, 0u);
}

TEST(ColorConfigValue, ParsesWhite) {
    CARL::ConfigValue<Color> cv {"color"};
    auto                     node = YAML::Load("color:\n  r: 255\n  g: 255\n  b: 255");
    cv.parse(node);
    EXPECT_EQ((*cv).r, 255u);
    EXPECT_EQ((*cv).g, 255u);
    EXPECT_EQ((*cv).b, 255u);
}

TEST(ColorConfigValue, MissingFieldFailsValidation) {
    CARL::ConfigValue<Color> cv {"color"};
    auto                     node = YAML::Load("other: 1");
    cv.parse(node);
    EXPECT_FALSE(cv.validate().correct);
}

TEST(ColorConfigValue, WithDefault) {
    Color                    red {255, 0, 0};
    CARL::ConfigValue<Color> cv {"color", CARL::Default<Color>{red}};
    EXPECT_TRUE(cv.validate().correct);
    EXPECT_EQ(*cv, red);
}

TEST(ColorConfigValue, DefaultOverwrittenByParse) {
    Color                    default_color {0, 0, 0};
    CARL::ConfigValue<Color> cv {"color", CARL::Default<Color>{default_color}};
    auto                     node = YAML::Load("color:\n  r: 10\n  g: 20\n  b: 30");
    cv.parse(node);
    EXPECT_EQ((*cv).r, 10u);
    EXPECT_EQ((*cv).g, 20u);
    EXPECT_EQ((*cv).b, 30u);
}

TEST(ColorConfigValue, PrintToContainsChannels) {
    CARL::ConfigValue<Color> cv {"color"};
    auto                     node = YAML::Load("color:\n  r: 10\n  g: 20\n  b: 30");
    cv.parse(node);
    std::ostringstream os;
    cv.printTo(os, "");
    std::string out = os.str();
    EXPECT_NE(out.find("10"), std::string::npos);
    EXPECT_NE(out.find("20"), std::string::npos);
    EXPECT_NE(out.find("30"), std::string::npos);
}

TEST(ColorConfigValue, MultilineValueIsReindentedInPrint) {
    // Color's operator<< produces multiple lines — ConfigValue must reindent them
    CARL::ConfigValue<Color> cv {"color"};
    auto                     node = YAML::Load("color:\n  r: 1\n  g: 2\n  b: 3");
    cv.parse(node);
    std::ostringstream os;
    cv.printTo(os, "  ");  // non-zero indent to trigger reindent
    std::string out = os.str();
    EXPECT_EQ(out.substr(0, 2), "  ");  // leading indent applied
    EXPECT_NE(out.find('\n'), std::string::npos);
}

// ============================================================
//  GeoPoint — ConfigValue<GeoPoint> parse
// ============================================================

TEST(GeoPointConfigValue, ParsesFromYaml) {
    CARL::ConfigValue<GeoPoint> cv {"position"};
    auto                        node = YAML::Load("position:\n  lat: 52.2297\n  lon: 21.0122\n  alt: 110.5");
    cv.parse(node);
    EXPECT_TRUE(cv.validate().correct);
    EXPECT_DOUBLE_EQ((*cv).lat, 52.2297);
    EXPECT_DOUBLE_EQ((*cv).lon, 21.0122);
    EXPECT_DOUBLE_EQ((*cv).alt, 110.5);
}

TEST(GeoPointConfigValue, ParsesNegativeCoordinates) {
    CARL::ConfigValue<GeoPoint> cv {"pos"};
    auto                        node = YAML::Load("pos:\n  lat: -33.8688\n  lon: 151.2093\n  alt: 0.0");
    cv.parse(node);
    EXPECT_TRUE(cv.validate().correct);
    EXPECT_NEAR((*cv).lat, -33.8688, 1e-6);
    EXPECT_NEAR((*cv).lon, 151.2093, 1e-6);
}

TEST(GeoPointConfigValue, MissingKeyFailsValidation) {
    CARL::ConfigValue<GeoPoint> cv {"position"};
    auto                        node = YAML::Load("other: 1");
    cv.parse(node);
    EXPECT_FALSE(cv.validate().correct);
}

TEST(GeoPointConfigValue, PrintContainsLatLonAlt) {
    CARL::ConfigValue<GeoPoint> cv {"position"};
    auto                        node = YAML::Load("position:\n  lat: 1.0\n  lon: 2.0\n  alt: 3.0");
    cv.parse(node);
    std::ostringstream os;
    cv.printTo(os, "");
    std::string out = os.str();
    EXPECT_NE(out.find("lat"), std::string::npos);
    EXPECT_NE(out.find("lon"), std::string::npos);
    EXPECT_NE(out.find("alt"), std::string::npos);
}

// ============================================================
//  Custom types inside ConfigGroup
// ============================================================

struct StationConfig : CARL::ConfigGroup
{
    CARL::ConfigValue<std::string> name {"name"};
    CARL::ConfigValue<GeoPoint> position {"position"};
    CARL::ConfigValue<Color> led {"led", CARL::Default<Color>{Color {0, 255, 0}}};
    StationConfig()
        : CARL::ConfigGroup("station") { registerEntries(name, position, led); }
};

TEST(CustomTypeInGroup, FullConfigParses) {
    StationConfig cfg;
    auto          node =
        YAML::Load(
        R"(
station:
  name: Warsaw-01
  position:
    lat: 52.2297
    lon: 21.0122
    alt: 110.0
  led:
    r: 0
    g: 200
    b: 0
)");
    cfg.parse(node);
    EXPECT_TRUE(cfg.validate().correct);
    EXPECT_EQ(*cfg.name, "Warsaw-01");
    EXPECT_DOUBLE_EQ((*cfg.position).lat, 52.2297);
    EXPECT_EQ((*cfg.led).g, 200u);
}

TEST(CustomTypeInGroup, DefaultColorAppliedWhenAbsent) {
    StationConfig cfg;
    auto          node =
        YAML::Load(R"(
station:
  name: Paris-02
  position:
    lat: 48.8566
    lon: 2.3522
    alt: 35.0
)");
    cfg.parse(node);
    EXPECT_TRUE(cfg.validate().correct);
    // led was absent — default {0, 255, 0} applies
    EXPECT_EQ((*cfg.led).r, 0u);
    EXPECT_EQ((*cfg.led).g, 255u);
    EXPECT_EQ((*cfg.led).b, 0u);
}

TEST(CustomTypeInGroup, RequiredCustomFieldMissingFailsValidation) {
    StationConfig cfg;
    auto          node = YAML::Load("station:\n  name: Broken"); // position missing
    cfg.parse(node);
    auto result = cfg.validate();
    EXPECT_FALSE(result.correct);
    bool position_error = false;
    for (auto const& e : result.errors) {
        if (e.find("position") != std::string::npos) {
            position_error = true;
        }
    }

    EXPECT_TRUE(position_error);
}

TEST(CustomTypeInGroup, PrintDoesNotCrash) {
    StationConfig cfg;
    auto          node = YAML::Load(R"(
station:
  name: Test
  position:
    lat: 0.0
    lon: 0.0
    alt: 0.0
)");
    cfg.parse(node);
    std::ostringstream os;
    EXPECT_NO_THROW(cfg.printTo(os, ""));
    EXPECT_NE(os.str().find("Test"), std::string::npos);
}

// ============================================================
//  Custom types inside ConfigMap
// ============================================================

struct SensorEntry : CARL::ConfigGroup
{
    CARL::ConfigValue<int> id {"id"};
    CARL::ConfigValue<GeoPoint> position {"position"};
    CARL::ConfigValue<Color> indicator {"indicator"};
    SensorEntry() { registerEntries(id, position, indicator); }
};

TEST(CustomTypeInMap, IdListWithCustomTypeParses) {
    CARL::ConfigMap<SensorEntry> map {"sensors", CARL::MapType::ID_LIST};
    auto                         node =
        YAML::Load(
        R"(
sensors:
  - id: 1
    position:
      lat: 50.0
      lon: 18.0
      alt: 200.0
    indicator:
      r: 255
      g: 0
      b: 0
  - id: 2
    position:
      lat: 51.0
      lon: 19.0
      alt: 210.0
    indicator:
      r: 0
      g: 255
      b: 0
)");
    EXPECT_NO_THROW(map.parse(node));
    EXPECT_TRUE(map.validate().correct);
}

TEST(CustomTypeInMap, ValidationErrorPrefixedCorrectly) {
    CARL::ConfigMap<SensorEntry> map {"sensors", CARL::MapType::ID_LIST};
    auto                         node =
        YAML::Load(R"(
sensors:
  - id: 1
    position:
      lat: 50.0
      lon: 18.0
      alt: 200.0
)");  // indicator missing from entry 1
    map.parse(node);
    auto result = map.validate();
    EXPECT_FALSE(result.correct);
    bool prefixed = false;
    for (auto const& e : result.errors) {
        if ((e.find("sensors[1]") != std::string::npos) && (e.find("indicator") != std::string::npos)) {
            prefixed = true;
        }
    }

    EXPECT_TRUE(prefixed);
}
