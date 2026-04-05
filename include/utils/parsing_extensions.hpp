#ifndef CARL_PARSING_EXTENSIONS_HPP
#define CARL_PARSING_EXTENSIONS_HPP

#include <yaml-cpp/yaml.h>

namespace YAML
{

template <typename Scalar, int Rows, int Cols>
struct convert<Eigen::Matrix<Scalar, Rows, Cols>>
{
    static bool decode(const Node& node, Eigen::Matrix<Scalar, Rows, Cols>& matrix)
    {
        // expect a list of lists:
        // matrix:
        //   - [1.0, 0.0, 0.0]
        //   - [0.0, 1.0, 0.0]
        //   - [0.0, 0.0, 1.0]
        if (!node.IsSequence() || (node.size() != Rows)) {
            return false;
        }

        for (int r = 0; r < Rows; ++r) {
            if (!node[r].IsSequence() || (node[r].size() != Cols)) {
                return false;
            }

            for (int c = 0; c < Cols; ++c) {
                matrix(r, c) = node[r][c].as<Scalar>();
            }
        }

        return true;
    }
};

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
