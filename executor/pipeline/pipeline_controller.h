//
// Created for Block Level Pipelining with Sliding Window Control
// Generic Pipeline Controller with Dynamic Resource-based Feedback
//

#ifndef NEUBLOCKCHAIN_PIPELINE_CONTROLLER_H
#define NEUBLOCKCHAIN_PIPELINE_CONTROLLER_H

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include <chrono>
#include <map>
#include <string>
#include "../../utilities/types/aria_types.h"

namespace pipeline {

// 资源类型
enum class ResourceType {
    CPU,           // CPU 计算能力
    MEMORY,        // 内存使用率
    DISK_IO,       // 磁盘 I/O
    NETWORK        // 网络带宽
};

// 资源监控配置
struct ResourceConfig {
    double min_utilization;  // U_r,min 健康工作区间下界
    double max_utilization;  // U_r,max 健康工作区间上界
    double weight;           // ω_{i,r} 资源权重
    
    ResourceConfig() 
        : min_utilization(0.0), max_utilization(1.0), weight(1.0) {}
    
    ResourceConfig(double min_util, double max_util, double w)
        : min_utilization(min_util), max_utilization(max_util), weight(w) {}
};

// 阶段状态
struct StageState {
    std::string stage_name;                            // 阶段名称
    size_t block_count;                                // 当前阶段区块数量 ζ_i
    size_t window_size;                                // 滑动窗口大小
    std::map<ResourceType, double> raw_utilization;    // 原始资源利用率 U_r
    double composite_utilization;                      // 综合资源负载 U_composite,i
    
    StageState() : block_count(0), window_size(1), composite_utilization(0.0) {}
};

// 阶段配置
struct StageConfig {
    std::string stage_name;                            // 阶段名称
    std::map<ResourceType, ResourceConfig> resources;  // 该阶段关注的资源及其配置
    size_t initial_window_size;                        // 初始窗口大小 ζ_i
    size_t max_window_size;                            // 最大窗口大小
    size_t min_window_size;                            // 最小窗口大小（通常为1）
    
    // 阈值配置
    double low_utilization_threshold;   // 低利用率阈值
    double high_utilization_threshold;  // 高利用率阈值
    size_t low_backlog_threshold;       // 低积压阈值
    size_t high_backlog_threshold;      // 高积压阈值
    
    // 调节系数
    double k_alpha;  // α增长系数基准值
    double k_beta;   // β减小系数基准值
    
    StageConfig()
        : initial_window_size(1),
          max_window_size(100),
          min_window_size(1),
          low_utilization_threshold(0.3),
          high_utilization_threshold(0.8),
          low_backlog_threshold(2),
          high_backlog_threshold(10),
          k_alpha(0.5),
          k_beta(0.3) {}
};

// 流水线统计信息
struct PipelineStats {
    size_t total_pipeline_length;      // μ = Σζ_i 流水线总长度
    size_t total_blocks_processed;     // 总处理区块数
    uint64_t total_processing_time_us; // 总处理时间（微秒）
    double avg_throughput;             // 平均吞吐量（blocks/sec）
    
    // 各阶段统计
    std::vector<StageState> stage_states;
    
    PipelineStats() 
        : total_pipeline_length(0),
          total_blocks_processed(0),
          total_processing_time_us(0),
          avg_throughput(0.0) {}
};

/**
 * PipelineController: 通用流水线控制器
 * 
 * 实现基于资源利用率和积压情况的动态滑动窗口算法：
 * 1. 归一化资源利用率: U'_r = (U_r - U_r,min) / (U_r,max - U_r,min)
 * 2. 计算综合负载: U_composite,i = Σ(ω_{i,r} * U'_r)
 * 3. 动态调整窗口:
 *    - 低负载低积压: ζ_{i-1} = ζ_{i-1} * (1 + α)
 *    - 适中负载: ζ_{i-1} = ζ_{i-1} + 1
 *    - 高负载高积压: ζ_{i-1} = ζ_{i-1} * (1 - β)
 * 4. 非线性调节: α = k_α * (1 - U_composite)^2, β = k_β * U_composite^2
 */
class PipelineController {
public:
    /**
     * 构造函数
     * @param stage_configs 各阶段配置列表（按执行顺序）
     */
    explicit PipelineController(const std::vector<StageConfig>& stage_configs);
    
    ~PipelineController() = default;
    
    // 禁止拷贝和移动
    PipelineController(const PipelineController&) = delete;
    PipelineController& operator=(const PipelineController&) = delete;
    PipelineController(PipelineController&&) = delete;
    PipelineController& operator=(PipelineController&&) = delete;
    
    /**
     * 更新阶段资源利用率
     * @param stage_index 阶段索引（0-based）
     * @param resource_type 资源类型
     * @param utilization 原始利用率 [0.0, 1.0]
     */
    void updateResourceUtilization(size_t stage_index, 
                                   ResourceType resource_type, 
                                   double utilization);
    
    /**
     * 更新阶段区块数量
     * @param stage_index 阶段索引
     * @param block_count 当前阶段的区块数量
     */
    void updateBlockCount(size_t stage_index, size_t block_count);
    
    /**
     * 执行滑动窗口调节算法
     * 根据当前阶段的资源利用率和积压情况，调整前一阶段的窗口大小
     */
    void adjustPipeline();
    
    /**
     * 获取指定阶段的窗口大小
     * @param stage_index 阶段索引
     * @return 当前窗口大小 ζ_i
     */
    size_t getWindowSize(size_t stage_index) const;
    
    /**
     * 获取流水线总长度
     * @return μ = Σζ_i
     */
    size_t getPipelineLength() const;
    
    /**
     * 获取流水线统计信息
     */
    PipelineStats getStats() const;
    
    /**
     * 打印流水线状态（用于调试）
     */
    void printStatus() const;
    
    /**
     * 重置流水线状态
     */
    void reset();
    
private:
    /**
     * 归一化资源利用率
     * U'_r = (U_r - U_r,min) / (U_r,max - U_r,min)
     */
    double normalizeUtilization(double raw_utilization, 
                               const ResourceConfig& config) const;
    
    /**
     * 计算阶段综合资源负载
     * U_composite,i = Σ(ω_{i,r} * U'_r)
     */
    double calculateCompositeUtilization(size_t stage_index);
    
    /**
     * 计算增长系数 α
     * α = k_α * (1 - U_composite,i)^2
     */
    double calculateAlpha(size_t stage_index, double composite_utilization) const;
    
    /**
     * 计算减小系数 β
     * β = k_β * U_composite,i^2
     */
    double calculateBeta(size_t stage_index, double composite_utilization) const;
    
    /**
     * 调整单个阶段的前驱窗口大小
     */
    void adjustStageWindow(size_t stage_index);
    
    // 配置和状态
    std::vector<StageConfig> stage_configs_;  // 各阶段配置
    std::vector<StageState> stage_states_;    // 各阶段运行状态
    
    // 统计信息
    std::atomic<size_t> total_blocks_processed_{0};
    std::atomic<uint64_t> total_processing_time_us_{0};
    
    // 线程安全
    mutable std::mutex mutex_;
};

/**
 * 默认阶段配置工厂
 * 提供常用的阶段配置模板
 */
class PipelineConfigFactory {
public:
    /**
     * 创建排序阶段配置
     * 特点：网络密集型
     */
    static StageConfig createOrderingStageConfig();
    
    /**
     * 创建无冲突执行阶段配置
     * 特点：CPU密集型
     */
    static StageConfig createNonConflictingExecutionConfig();
    
    /**
     * 创建无冲突提交阶段配置
     * 特点：磁盘I/O密集型
     */
    static StageConfig createNonConflictingCommitConfig();
    
    /**
     * 创建检查点阶段配置
     * 特点：网络和存储同步
     */
    static StageConfig createCheckpointConfig();
    
    /**
     * 创建冲突执行和提交阶段配置
     * 特点：CPU和内存密集型，带I/O
     */
    static StageConfig createConflictingExecutionCommitConfig();
    
    /**
     * 创建完整的五阶段流水线配置
     */
    static std::vector<StageConfig> createDefaultPipelineConfig();
};

} // namespace pipeline

#endif // NEUBLOCKCHAIN_PIPELINE_CONTROLLER_H