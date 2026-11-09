#include "jenks.hh"
#include <cmath>
#include <limits>
#include <numeric>

std::vector<double> JenksNaturalBreaks::compute_breaks(std::vector<double> values, size_t num_classes) {
    if (values.empty()) {
        return std::vector<double>(num_classes, 0.0);
    }

    std::sort(values.begin(), values.end());
    size_t num_data = values.size();

    if (num_classes >= num_data) {
        std::vector<double> breaks(num_classes, values.back());
        for (size_t i = 0; i < num_data; ++i) {
            breaks[i] = values[i];
        }
        return breaks;
    }

    // Dynamic programming matrices:
    // mat1[i][j]: variance error of first i items divided into j classes
    // mat2[i][j]: index of the split for class j
    std::vector<std::vector<double>> mat1(num_data + 1, std::vector<double>(num_classes + 1, 0.0));
    std::vector<std::vector<int>> mat2(num_data + 1, std::vector<int>(num_classes + 1, 0));

    for (size_t y = 1; y <= num_classes; ++y) {
        mat1[0][y] = 0.0;
        mat1[1][y] = 0.0;
        mat2[0][y] = 0;
        mat2[1][y] = 1;
        for (size_t i = 2; i <= num_data; ++i) {
            mat1[i][y] = std::numeric_limits<double>::infinity();
        }
    }

    for (size_t l = 2; l <= num_data; ++l) {
        double s1 = 0.0;
        double s2 = 0.0;
        double w = 0.0;
        for (size_t m = 1; m <= l; ++m) {
            size_t i3 = l - m + 1;
            double val = values[i3 - 1];
            s2 += val * val;
            s1 += val;
            w += 1.0;
            double v = s2 - (s1 * s1) / w;
            size_t i4 = i3 - 1;
            if (i4 != 0) {
                for (size_t j = 2; j <= num_classes; ++j) {
                    if (mat1[l][j] >= (v + mat1[i4][j - 1])) {
                        mat2[l][j] = static_cast<int>(i3);
                        mat1[l][j] = v + mat1[i4][j - 1];
                    }
                }
            }
        }
        mat1[l][1] = s2 - (s1 * s1) / w;
        mat2[l][1] = 1;
    }

    // Backtrack class breaks safely
    std::vector<double> breaks(num_classes, values.back());
    int k = static_cast<int>(num_data);
    for (int j = static_cast<int>(num_classes); j >= 2; --j) {
        if (k < 1) break;
        int split = mat2[k][j];
        int id = split - 2;
        if (id < 0) id = 0;
        if (id < static_cast<int>(num_data)) {
            breaks[j - 2] = values[id];
        }
        k = split - 1;
    }

    return breaks;
}

uint8_t JenksNaturalBreaks::get_class(double value, const std::vector<double>& breaks) {
    if (breaks.empty()) return 0;
    for (size_t i = 0; i < breaks.size(); ++i) {
        if (value <= breaks[i]) {
            return static_cast<uint8_t>(i);
        }
    }
    return static_cast<uint8_t>(breaks.size() - 1);
}
