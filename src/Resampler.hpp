#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

namespace rmx
{
class Resampler
{
public:
    static constexpr int radius = 64;
    static constexpr int phases = 512;

    void prepare(double sourceRate, double destinationRate)
    {
        const auto cutoff = std::min({18335.0, sourceRate * .474, destinationRate * .474}) / sourceRate;
        kernels.resize(phases + 1);
        for (int p = 0; p <= phases; ++p)
        {
            const double frac = static_cast<double>(p) / phases;
            double sum = 0.0;
            for (int i = -radius; i <= radius; ++i)
            {
                const double x = i - frac;
                const double a = std::abs(x) / (radius + 1.0);
                const double win = .42 + .5 * std::cos(std::numbers::pi * a)
                                      + .08 * std::cos(2.0 * std::numbers::pi * a);
                const double z = 2.0 * std::numbers::pi * cutoff * x;
                const auto value = 2.0 * cutoff * win * (std::abs(z) < 1e-10 ? 1.0 : std::sin(z) / z);
                kernels[p][i + radius] = static_cast<float>(value);
                sum += value;
            }
            for (auto& coefficient : kernels[p])
                coefficient /= static_cast<float>(sum);
        }
        reset();
    }

    void reset() { data = {}; }
    void push(int64_t index, const std::array<float, 2>& value)
    {
        const auto at = static_cast<size_t>(index) & mask;
        for (int ch = 0; ch < 2; ++ch)
            data[ch][at] = data[ch][at + size] = value[ch];
    }

    std::array<float, 2> get(double position) const
    {
        const auto base = static_cast<int64_t>(std::floor(position));
        const auto phase = std::clamp(static_cast<int>((position - base) * phases + .5), 0, phases);
        const auto start = static_cast<size_t>(base - radius) & mask;
        const auto& taps = kernels[phase];
        std::array<float, 2> result {};
        for (size_t i = 0; i < taps.size(); ++i)
        {
            result[0] += data[0][start + i] * taps[i];
            result[1] += data[1][start + i] * taps[i];
        }
        return result;
    }

private:
    static constexpr size_t size = 512, mask = size - 1;
    std::array<std::array<float, size * 2>, 2> data {};
    std::vector<std::array<float, radius * 2 + 1>> kernels;
};

}
