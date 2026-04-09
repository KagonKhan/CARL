#ifndef CARL_CAMERA_CONFIG_HPP
#define CARL_CAMERA_CONFIG_HPP

#include "../include/config_group.hpp"
#include "../include/config_map.hpp"
#include "../include/utils/parsing_extensions.hpp"

namespace CARL
{

class GeneralConfig : public ConfigGroup
{
public:

private:
    ConfigValue<std::string> motylAddress_ {"motylAddress"};
};


class PtzCamerasModelsConfig : public ConfigGroup
{
public:
    class CameraModel : public ConfigGroup
    {
    public:
        CameraModel()
        {
            register_(
                tiltRange_,
                panRange_,
                focalLength_,
                resolution_,
                pixelDimensions_,
                sensorDimensions_,
                opticalZoom_);
        }

    private:
        ConfigValue<Range<int>> tiltRange_ {"tiltRange"};
        ConfigValue<Range<int>> panRange_ {"panRange"};
        ConfigValue<Range<int>> focalLength_ {"focalLength"};

        ConfigValue<Size2<int>> resolution_ {"resolution"};
        ConfigValue<Size2<int>> pixelDimensions_ {"pixelDimensions"};
        ConfigValue<Size2<int>> sensorDimensions_ {"sensorDimensions"};

        ConfigValue<int> opticalZoom_ {"opticalZoom"};
    };
public:
    PtzCamerasModelsConfig()
        : ConfigGroup("ptzCamerasModels") { register_(models_); }

private:
    ConfigMap<CameraModel, std::string> models_ {""};
};


class DatabaseConfig : public ConfigGroup
{
public:
    DatabaseConfig()
        : ConfigGroup("database")
    {
        register_(server_, user_, password_, name_, mediaHash_, port_);
    }

private:
    ConfigValue<std::string> server_ {"server"};
    ConfigValue<std::string> user_ {"user"};
    ConfigValue<std::string> password_ {"password"};
    ConfigValue<std::string> name_ {"name"};
    ConfigValue<std::string> mediaHash_ {"mediaHash"};
    ConfigValue<int>         port_ {"port"};
};

class CamerasConfig : public ConfigGroup
{
public:
    class Camera : public ConfigGroup
    {
    public:
        Camera() { register_(width_, height_, horizontalAngle_, verticalAngle_, vssWidth_, vssHeight_, f_); }
    private:
        ConfigValue<int> width_ {"width"};
        ConfigValue<int> height_ {"height"};
        ConfigValue<int> horizontalAngle_ {"horizontalAngle"};
        ConfigValue<int> verticalAngle_ {"verticalAngle"};
        ConfigValue<int> vssWidth_ {"vssWidth"};
        ConfigValue<int> vssHeight_ {"vssHeight"};
        ConfigValue<int> f_ {"f"};
    };
public:
    CamerasConfig()
        : ConfigGroup("cameras") { register_(cameras_); }

private:
    ConfigMap<Camera, std::string> cameras_ {""};
};


class PtzCameraConfig : public ConfigGroup
{
public:
    class SensorSettings : public ConfigGroup
    {
    public:
        SensorSettings()
            : ConfigGroup("sensorSettings")
        {
            register_(maxExposureTime_, spatialNoiseFilter_, temporalNoiseFilter_, localContrast_, toneMapping_);
        }

    private:
        ConfigValue<int> maxExposureTime_ {"maxExposureTime"};
        ConfigValue<int> spatialNoiseFilter_ {"spatialNoiseFilter"};
        ConfigValue<int> temporalNoiseFilter_ {"temporalNoiseFilter"};
        ConfigValue<int> localContrast_ {"localContrast"};
        ConfigValue<int> toneMapping_ {"toneMapping"};
    };
    class Calibration : public ConfigGroup
    {
    public:
        class Sinusoidal : public ConfigGroup
        {
        public:
            struct Values : ConfigGroup
            {
                explicit Values(std::string name)
                    : ConfigGroup(name)
                {
                    register_(a_, b_, c_, d_, e_, f_, g_, h_, i_);
                }

                ConfigValue<int> a_ {"a"}, b_ {"b"}, c_ {"c"}, d_ {"d"}, e_ {"e"}, f_ {"f"}, g_ {"g"}, h_ {"h"},
                i_ {"i"};
            };
        public:
            Sinusoidal()
                : ConfigGroup("sinusoidal")
            {
                register_(sin, pan);
            }

        private:
            Values sin {"tilt"};
            Values pan {"pan"};
        };
    public:
        Calibration()
            : ConfigGroup("calibration")
        {
            register_(panAdjustment_, tiltAdjustment_, cameraPosition_, sin_);
        }

    private:
        ConfigValue<int>              panAdjustment_ {"panAdjustment"};
        ConfigValue<int>              tiltAdjustment_ {"tiltAdjustment"};
        ConfigValue<std::vector<int>> cameraPosition_ {"cameraPosition"};
        Sinusoidal                    sin_;
    };

public:
    PtzCameraConfig() { register_(id_, ip_, user_, password_, model_, sensorSettings_, calibration_); }

private:
    ConfigValue<int>         id_ {"id"};
    ConfigValue<std::string> ip_ {"ip"};
    ConfigValue<std::string> user_ {"user"};
    ConfigValue<std::string> password_ {"password"};
    ConfigValue<std::string> model_ {"model"};

    SensorSettings sensorSettings_;
    Calibration    calibration_;
};

class StationConfig : public ConfigGroup
{
public:
    StationConfig() { register_(id_, name_, ptzCameraConfig_); }

private:
    ConfigValue<int>           id_ {"id"};
    ConfigValue<std::string>   name_ {"name"};
    ConfigMap<PtzCameraConfig> ptzCameraConfig_ {"ptzCameras", MapType::ID_LIST};
};


class StationsConfig : public ConfigGroup
{
public:
    StationsConfig() { register_(stations_); }

private:
    ConfigMap<StationConfig> stations_ {"stations", MapType::ID_LIST};
};


class Config : public ConfigGroup
{
public:
    Config() { register_(general_, ptzCamerasModels_, database_, cameras_, stations_); }

private:
    GeneralConfig          general_;
    PtzCamerasModelsConfig ptzCamerasModels_;
    DatabaseConfig         database_;
    CamerasConfig          cameras_;
    StationsConfig         stations_;
};

} // namespace CARL

#endif // CARL_CAMERA_CONFIG_HPP
