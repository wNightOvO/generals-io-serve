/*
    随机数生成器
    全局只使用一个随机数生成器 方便固定种子复现
    种子由 main 函数或 runner 设置 未设置时使用默认种子
*/

#pragma once
#include <random>
#include <cstdint>
#include <chrono>
#include <utility>

namespace gen {

// 默认随机种子
inline constexpr std::uint32_t DEFAULT_SEED = 0x3f77a690;

// 当前随机种子
inline std::uint32_t randomSeedValue = DEFAULT_SEED;

// 全局随机数生成器
inline std::mt19937 randomEngine(DEFAULT_SEED);

// 设置随机种子 参数 settingSeed 为新的种子
inline void setSeed(std::uint32_t settingSeed) {
    randomSeedValue = settingSeed;
    randomEngine.seed(settingSeed);
}

// 用当前时间产生一个随机种子 用于未指定种子时
inline std::uint32_t makeTimeSeed() {
    std::uint64_t now = static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uint32_t high = static_cast<std::uint32_t>(now >> 32);
    std::uint32_t low = static_cast<std::uint32_t>(now);
    return high ^ (low * 2654435761u);
}

// 返回闭区间 min 到 max 内的随机整数 参数顺序不要求
inline int randomInt(int min, int max) {
    if(min > max) std::swap(min, max);
    std::uniform_int_distribution<int> distribution(min, max);
    return distribution(randomEngine);
}

// 返回 0 到 1 之间的随机浮点数 左闭右开
inline double random01() {
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(randomEngine);
}

// 返回 low 到 high 之间的随机浮点数
inline double randomDouble(double low, double high) {
    std::uniform_real_distribution<double> distribution(low, high);
    return distribution(randomEngine);
}

// 以概率 probability 返回 true 参数 probability 取值 0 到 1
inline bool randomChance(double probability) {
    return random01() < probability;
}

// 随机打乱区间内的元素
template<class Iterator>
void randomShuffle(Iterator begin, Iterator end) {
    std::shuffle(begin, end, randomEngine);
}

} // namespace gen
