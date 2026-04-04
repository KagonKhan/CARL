#include "configs/camera_config.hpp"
#include "include/config_group.hpp"
#include "include/config_value.hpp"
using namespace CARL;

#include <iostream>


int main()
{
    CameraConfig camera;

    YAML::Node node = YAML::LoadFile("/home/sszynk/projects/CARL/config.yaml");
    camera.parse(node);
    auto result = camera.validate();

    if (!result.correct) {
        for (auto error : result.errors) {
            std::cout << error << std::endl;
        }
    }

    camera.printTo(std::cout);

    // std::cout << *test_entry << "\n";
}
