#ifndef CARL_PARSING_EXTENSIONS_HPP
#define CARL_PARSING_EXTENSIONS_HPP

#include <fmt/core.h>
#include <yaml-cpp/yaml.h>

template <typename T>
struct Range
{
    T min;
    T max;
};

template <typename T>
struct Size2
{
    T width;
    T height;
};


template <typename T>
std::ostream& operator <<(std::ostream& os, const std::vector<T>& vec)
{
    os << '[';

    for (size_t i = 0; i < vec.size(); ++i) {
        os << vec[i];
        if (i + 1 < vec.size()) {
            os << ", ";
        }
    }

    os << ']';
    return os;
}

template <typename T>
std::ostream& operator <<(std::ostream& os, Size2<T> const& size)
{
    os << "width: " << size.width << "\nheight: " << size.height;
    return os;
}

template <typename T>
std::ostream& operator <<(std::ostream& os, Range<T> const& size)
{
    os << "min: " << size.min << "\nmax: " << size.max;
    return os;
}

namespace YAML
{

template <typename T>
struct convert<Range<T>>
{
    static bool decode(Node const& node, Range<T>& range)
    {
        range.min = node["min"].as<T>();
        range.max = node["max"].as<T>();

        return true;
    }
};

template <typename T>
struct convert<Size2<T>>
{
    static bool decode(Node const& node, Size2<T>& range)
    {
        range.width  = node["width"].as<T>();
        range.height = node["height"].as<T>();

        return true;
    }
};


// template <typename Scalar, int Rows, int Cols>
// struct convert<Eigen::Matrix<Scalar, Rows, Cols>>
// {
//     static bool decode(const Node& node, Eigen::Matrix<Scalar, Rows, Cols>& matrix)
//     {
//         // expect a list of lists:
//         // matrix:
//         //   - [1.0, 0.0, 0.0]
//         //   - [0.0, 1.0, 0.0]
//         //   - [0.0, 0.0, 1.0]
//         if (!node.IsSequence() || (node.size() != Rows)) {
//             return false;
//         }

//         for (int r = 0; r < Rows; ++r) {
//             if (!node[r].IsSequence() || (node[r].size() != Cols)) {
//                 return false;
//             }

//             for (int c = 0; c < Cols; ++c) {
//                 matrix(r, c) = node[r][c].as<Scalar>();
//             }
//         }

//         return true;
//     }
// };

// Vector is just a special case of Matrix, so this covers
// Eigen::Vector3f, Eigen::Vector4d, etc. for free


template <typename T>
struct convert<std::vector<T>>
{
    static bool decode(const Node& node, std::vector<T>& vec)
    {
        if (!node.IsSequence()) {
            return false;
        }

        vec.clear();
        for (const auto& item : node) {
            vec.push_back(item.as<T>());
        }

        return true;
    }
};

} // namespace YAML

#endif // CARL_PARSING_EXTENSIONS_HPP
