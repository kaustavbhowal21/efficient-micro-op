#ifndef JENKS_HH
#define JENKS_HH

#include <vector>
#include <cstdint>
#include <algorithm>

class JenksNaturalBreaks {
public:
    // Calculates k class breaks (k-1 cut points) for a vector of values
    // Returns a vector of break thresholds of size k
    // The classes will be:
    //   Class 0: values <= breaks[0]
    //   Class 1: breaks[0] < values <= breaks[1]
    //   ...
    //   Class k-1: breaks[k-2] < values <= breaks[k-1]
    static std::vector<double> compute_breaks(std::vector<double> values, size_t num_classes);

    // Given breaks and a value, returns the cluster/weight index (0 .. num_classes-1)
    static uint8_t get_class(double value, const std::vector<double>& breaks);
};

#endif // JENKS_HH

