#include "configs/PtzCamerasModels.hpp"
#include "include/config_group.hpp"
#include "include/config_map.hpp"
#include "include/config_value.hpp"
using namespace CARL;

#include <iostream>


int main()
{
    Config cameras {};

    YAML::Node node = YAML::LoadFile("/home/sszynk/projects/CARL/config.yaml");
    cameras.parse(node);
    auto result = cameras.validate();

    if (!result.correct) {
        for (auto error : result.errors) {
            std::cout << error << std::endl;
        }
    }

    cameras.printTo(std::cout);

    // std::cout << *test_entry << "\n";
}
