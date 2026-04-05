#ifndef CARL_CAMERA_CONFIG_HPP
#define CARL_CAMERA_CONFIG_HPP

#include "../include/config_group.hpp"


namespace CARL
{

class CameraConfig : public ConfigGroup
{
public:
    class CalibrationConfig : public ConfigGroup
    {
    public:
        CalibrationConfig()
            : ConfigGroup("calibration", Required::YES)
        {
            register_(&fx_);
            register_(&fy_);
        }

        ConfigValue<float> fx_ {"fx"};
        ConfigValue<float> fy_ {"fy"};
    };

public:
    CameraConfig()
        : ConfigGroup("")
    {
        register_(&width_);
        register_(&height_);
        register_(&fov_);
        register_(&calibration_);
    }

    int   width() const  { return *width_; }
    int   height() const { return *height_; }
    float fov() const    { return *fov_; }

    CalibrationConfig const& calibration() const { return calibration_; }

private:
    ConfigValue<int>   width_ {"width"};
    ConfigValue<int>   height_ {"height"};
    ConfigValue<float> fov_ {"fov", Default {90.0f}};
    CalibrationConfig  calibration_;
};

} // namespace CARL

#endif // CARL_CAMERA_CONFIG_HPP
