#include <vector>
#include <random>
#include <algorithm>
#include "schema.h"


/*
	随机返回sampleCount个记录，用于查询的limit
*/

void Fisher_Yates(std::vector<media_elem>& source, size_t sampleCount)
{
    const size_t total = source.size();
    if (sampleCount >= total) return;   // 无需修改

    // 静态随机数生成器（只初始化一次）
    static std::random_device rd;
    static std::mt19937 gen(rd());

    // 对前 sampleCount 个位置进行部分洗牌（只打乱前 sampleCount 个位置）
    // 或者对整个向量进行洗牌，然后 resize
    std::shuffle(source.begin(), source.end(), gen);
    source.resize(sampleCount);
}

std::vector<media_elem> limitRandom(const std::vector<media_elem>& source, size_t sampleCount)
{
    const size_t total = source.size();
    std::vector<media_elem> result;

    // 如果要取的数量 ≥ 总数据，直接返回全部
    if (sampleCount >= total)
    {
        result = source;
        return result;
    }
	// 预留至少 n 个元素的存储空间
    result.reserve(sampleCount);

    // 随机引擎，static 只初始化一次（性能更好）
	// std::random_device 仅用于初始化种子，不影响循环性能。
    static std::random_device rd;
	// std::mt19937 是高效的伪随机数生成器，每次生成一个 32 位整数非常快。
    static std::mt19937 gen(rd());

	// 算法遍历源向量一次，执行 O(n) 次迭代，其中 n = source.size()。
    // 第一步：先把前 sampleCount 个放入蓄水池
    for (size_t i = 0; i < sampleCount; ++i)
    {
        result.push_back(source[i]);
    }

    // 第二步：遍历剩下所有元素
    for (size_t i = sampleCount; i < total; ++i)
    {
        // 在 [0, i] 范围内生成随机下标
		// 一次随机整数生成（std::uniform_int_distribution + mt19937）。
        // std::uniform_int_distribution 在生成范围内会有少量额外计算（如拒绝采样），但开销很小。
		std::uniform_int_distribution<size_t> dist(0, i);
        size_t idx = dist(gen);

        // 落到蓄水池范围内，则替换
        if (idx < sampleCount)
        {
            result[idx] = source[i];
        }
    }

    return result;
}