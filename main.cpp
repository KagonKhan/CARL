#include "include/config_value.hpp"

using namespace CARL;

#include <iostream>

int main()
{
    ConfigValue<int> test_entry("TestEntry", Default {5});

    YAML::Node node = YAML::Load("TestEntry: lmao");

    test_entry.parse(node);


    std::cout << *test_entry << "\n";
}
