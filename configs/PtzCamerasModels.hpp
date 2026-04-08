#ifndef CARL_CAMERA_CONFIG_HPP
#define CARL_CAMERA_CONFIG_HPP

#include "../include/config_group.hpp"
#include "../include/config_map.hpp"
#include "../include/utils/parsing_extensions.hpp"

namespace CARL
{

class PtzCamerasModels : public ConfigGroup
{
public:
    class CameraModel : public ConfigGroup
    {
    public:
        CameraModel()
            : ConfigGroup("", Required::YES)
        {
            register_(&tiltRange_);
            register_(&panRange_);
            register_(&focalLength_);

            register_(&resolution_);
            register_(&pixelDimensions_);
            register_(&sensorDimensions_);

            register_(&opticalZoom_);
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
    PtzCamerasModels()
        : ConfigGroup("")
    {
        register_(&models_);
    }

private:
    ConfigMap<CameraModel, std::string> models_ {"ptzCamerasModels"};
};


class DatabaseConfig : public ConfigGroup
{
public:


public:
    DatabaseConfig()
        : ConfigGroup("database")
    {
        register_(&server_);
        register_(&user_);
        register_(&password_);
        register_(&name_);
        register_(&mediaHash_);
        register_(&port_);
    }

private:
    ConfigValue<std::string> server_ {"server"};
    ConfigValue<std::string> user_ {"user"};
    ConfigValue<std::string> password_ {"password"};
    ConfigValue<std::string> name_ {"name"};
    ConfigValue<std::string> mediaHash_ {"mediaHash"};
    ConfigValue<int>         port_ {"port"};
};


class ptzCameraConfig : public ConfigGroup
{
public:
    class SensorSettings : public ConfigGroup
    {
    public:
        SensorSettings()
            : ConfigGroup("sensorSettings")
        {
            register_(&maxExposureTime_);
            register_(&spatialNoiseFilter_);
            register_(&temporalNoiseFilter_);
            register_(&localContrast_);
            register_(&toneMapping_);
        }

    private:
        ConfigValue<int> maxExposureTime_ {"maxExposureTime"};
        ConfigValue<int> spatialNoiseFilter_ {"spatialNoiseFilter"};
        ConfigValue<int> temporalNoiseFilter_ {"temporalNoiseFilter"};
        ConfigValue<int> localContrast_ {"localContrast"};
        ConfigValue<int> toneMapping_ {"toneMapping"};
    };

public:
    ptzCameraConfig() { register_(id_, ip_, user_, password_, model_, sensorSettings_); }

private:
    ConfigValue<int>         id_ {"id"};
    ConfigValue<std::string> ip_ {"ip"};
    ConfigValue<std::string> user_ {"user"};
    ConfigValue<std::string> password_ {"password"};
    ConfigValue<std::string> model_ {"model"};

    SensorSettings sensorSettings_;
};

class StationConfig : public ConfigGroup
{
public:
    StationConfig() { register_(id_, name_, ptzCameraConfig_); }

private:
    ConfigValue<int>           id_ {"id"};
    ConfigValue<std::string>   name_ {"name"};
    ConfigMap<ptzCameraConfig> ptzCameraConfig_ {"ptzCameras", MapType::ID_LIST};
};


class StationsConfig : public ConfigGroup
{
public:
    StationsConfig() { register_(stations_); }

private:
    ConfigMap<StationConfig> stations_ {"stations", MapType::ID_LIST};
};

} // namespace CARL

#endif // CARL_CAMERA_CONFIG_HPP
