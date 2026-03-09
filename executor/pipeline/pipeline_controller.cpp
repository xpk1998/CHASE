//
// Created for Block Level Pipelining with Sliding Window Control
// Pipeline Controller Implementation
//

#include "pipeline_controller.h"
#include <glog/logging.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace pipeline {

// ============================================================================
// PipelineController Implementation
// ============================================================================

PipelineController::PipelineController(const std::vector<StageConfig>& stage_configs)
    : stage_configs_(stage_configs) {
    
    if (stage_configs_.empty()) {
        LOG(ERROR) << "PipelineController: Cannot create pipeline with empty stage configs";
        throw std::invalid_argument("stage_configs cannot be empty");
    }
    
    // 初始化各阶段状态
    stage_states_.resize(stage_configs_.size());
    for (size_t i = 0; i < stage_configs_.size(); ++i) {
        stage_states_[i].stage_name = stage_configs_[i].stage_name;
        stage_states_[i].window_size = stage_configs_[i].initial_window_size;
        stage_states_[i].block_count = 0;
        stage_states_[i].composite_utilization = 0.0;
    }
    
    LOG(INFO) << "PipelineController initialized with " << stage_configs_.size() << " stages";
}

void PipelineController::updateResourceUtilization(size_t stage_index, 
                                                   ResourceType resource_type, 
                                                   double utilization) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (stage_index >= stage_states_.size()) {
        LOG(WARNING) << "Invalid stage index: " << stage_index;
        return;
    }
    
    // 更新原始资源利用率
    stage_states_[stage_index].raw_utilization[resource_type] = utilization;
    
    DLOG(INFO) << "Stage [" << stage_states_[stage_index].stage_name << "] "
               << "Resource utilization updated - "
               << "Type: " << static_cast<int>(resource_type) 
               << ", Value: " << utilization;
}

void PipelineController::updateBlockCount(size_t stage_index, size_t block_count) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (stage_index >= stage_states_.size()) {
        LOG(WARNING) << "Invalid stage index: " << stage_index;
        return;
    }
    
    stage_states_[stage_index].block_count = block_count;
    
    DLOG(INFO) << "Stage [" << stage_states_[stage_index].stage_name << "] "
               << "Block count updated: " << block_count;
}

void PipelineController::adjustPipeline() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 从后向前调整各阶段（除了第一个阶段）
    for (size_t i = 1; i < stage_states_.size(); ++i) {
        adjustStageWindow(i);
    }
    
    DLOG(INFO) << "Pipeline adjustment completed. Total length: " << getPipelineLength();
}

size_t PipelineController::getWindowSize(size_t stage_index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (stage_index >= stage_states_.size()) {
        LOG(WARNING) << "Invalid stage index: " << stage_index;
        return 0;
    }
    
    return stage_states_[stage_index].window_size;
}

size_t PipelineController::getPipelineLength() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t total_length = 0;
    for (const auto& state : stage_states_) {
        total_length += state.block_count;
    }
    
    return total_length;
}

PipelineStats PipelineController::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    PipelineStats stats;
    stats.total_pipeline_length = 0;
    
    for (const auto& state : stage_states_) {
        stats.total_pipeline_length += state.block_count;
        stats.stage_states.push_back(state);
    }
    
    stats.total_blocks_processed = total_blocks_processed_.load();
    stats.total_processing_time_us = total_processing_time_us_.load();
    
    if (stats.total_processing_time_us > 0) {
        stats.avg_throughput = static_cast<double>(stats.total_blocks_processed) * 1000000.0 
                              / stats.total_processing_time_us;
    }
    
    return stats;
}

void PipelineController::printStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "\n========== Pipeline Status ==========\n";
    oss << "Total Pipeline Length (μ): " << getPipelineLength() << "\n";
    oss << "Total Blocks Processed: " << total_blocks_processed_.load() << "\n";
    oss << "\nStage Details:\n";
    
    for (size_t i = 0; i < stage_states_.size(); ++i) {
        const auto& state = stage_states_[i];
        oss << "  [" << i << "] " << state.stage_name << ":\n";
        oss << "      Blocks (ζ_" << i << "): " << state.block_count << "\n";
        oss << "      Window Size: " << state.window_size << "\n";
        oss << "      Composite Utilization: " << std::fixed << std::setprecision(3) 
            << state.composite_utilization << "\n";
        
        if (!state.raw_utilization.empty()) {
            oss << "      Resource Utilization:\n";
            for (const auto& [res_type, util] : state.raw_utilization) {
                oss << "        - Type " << static_cast<int>(res_type) 
                    << ": " << std::fixed << std::setprecision(3) << util << "\n";
            }
        }
    }
    
    oss << "=====================================\n";
    
    LOG(INFO) << oss.str();
}

void PipelineController::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (size_t i = 0; i < stage_states_.size(); ++i) {
        stage_states_[i].block_count = 0;
        stage_states_[i].window_size = stage_configs_[i].initial_window_size;
        stage_states_[i].composite_utilization = 0.0;
        stage_states_[i].raw_utilization.clear();
    }
    
    total_blocks_processed_.store(0);
    total_processing_time_us_.store(0);
    
    LOG(INFO) << "Pipeline reset completed";
}

// ============================================================================
// Private Methods
// ============================================================================

double PipelineController::normalizeUtilization(double raw_utilization, 
                                                const ResourceConfig& config) const {
    // U'_r = (U_r - U_r,min) / (U_r,max - U_r,min)
    double denominator = config.max_utilization - config.min_utilization;
    
    if (std::abs(denominator) < 1e-6) {
        return 0.0;  // 避免除零
    }
    
    double normalized = (raw_utilization - config.min_utilization) / denominator;
    
    // 限制在 [0, 1] 范围内
    return std::max(0.0, std::min(1.0, normalized));
}

double PipelineController::calculateCompositeUtilization(size_t stage_index) {
    if (stage_index >= stage_configs_.size()) {
        return 0.0;
    }
    
    const auto& config = stage_configs_[stage_index];
    const auto& state = stage_states_[stage_index];
    
    // U_composite,i = Σ(ω_{i,r} * U'_r)
    double composite = 0.0;
    double total_weight = 0.0;
    
    for (const auto& [res_type, res_config] : config.resources) {
        auto it = state.raw_utilization.find(res_type);
        if (it != state.raw_utilization.end()) {
            double normalized = normalizeUtilization(it->second, res_config);
            composite += res_config.weight * normalized;
            total_weight += res_config.weight;
        }
    }
    
    // 归一化权重和
    if (total_weight > 0.0) {
        composite /= total_weight;
    }
    
    // 更新状态
    stage_states_[stage_index].composite_utilization = composite;
    
    return composite;
}

double PipelineController::calculateAlpha(size_t stage_index, 
                                         double composite_utilization) const {
    if (stage_index >= stage_configs_.size()) {
        return 0.0;
    }
    
    // α = k_α * (1 - U_composite,i)^2
    double k_alpha = stage_configs_[stage_index].k_alpha;
    double diff = 1.0 - composite_utilization;
    
    return k_alpha * diff * diff;
}

double PipelineController::calculateBeta(size_t stage_index, 
                                        double composite_utilization) const {
    if (stage_index >= stage_configs_.size()) {
        return 0.0;
    }
    
    // β = k_β * U_composite,i^2
    double k_beta = stage_configs_[stage_index].k_beta;
    
    return k_beta * composite_utilization * composite_utilization;
}

void PipelineController::adjustStageWindow(size_t stage_index) {
    if (stage_index == 0 || stage_index >= stage_configs_.size()) {
        return;  // 第一个阶段没有前驱，不需要调整
    }
    
    const auto& config = stage_configs_[stage_index];
    const auto& state = stage_states_[stage_index];
    
    // 计算综合资源负载
    double composite_util = calculateCompositeUtilization(stage_index);
    
    // 获取前一阶段的窗口大小
    size_t prev_index = stage_index - 1;
    size_t& prev_window = stage_states_[prev_index].window_size;
    const auto& prev_config = stage_configs_[prev_index];
    
    // 获取当前积压（当前阶段的区块数量）
    size_t backlog = state.block_count;
    
    // 决策逻辑
    if (composite_util < config.low_utilization_threshold && 
        backlog < config.low_backlog_threshold) {
        // 情况1：资源利用率较低且积压接近零 -> 增加前一阶段窗口
        double alpha = calculateAlpha(stage_index, composite_util);
        size_t new_window = static_cast<size_t>(prev_window * (1.0 + alpha));
        prev_window = std::min(new_window, prev_config.max_window_size);
        
        DLOG(INFO) << "Stage [" << state.stage_name << "] low utilization (" 
                   << composite_util << ") -> Increase prev window to " << prev_window
                   << " (α=" << alpha << ")";
        
    } else if (composite_util > config.high_utilization_threshold || 
               backlog > config.high_backlog_threshold) {
        // 情况3：资源利用率接近上限或积压过多 -> 减小前一阶段窗口
        double beta = calculateBeta(stage_index, composite_util);
        size_t new_window = static_cast<size_t>(prev_window * (1.0 - beta));
        prev_window = std::max(new_window, prev_config.min_window_size);
        
        DLOG(INFO) << "Stage [" << state.stage_name << "] high utilization (" 
                   << composite_util << ") or backlog (" << backlog 
                   << ") -> Decrease prev window to " << prev_window
                   << " (β=" << beta << ")";
        
    } else {
        // 情况2：资源利用率和积压适中 -> 适度增加窗口
        prev_window = std::min(prev_window + 1, prev_config.max_window_size);
        
        DLOG(INFO) << "Stage [" << state.stage_name << "] moderate utilization (" 
                   << composite_util << ") -> Increment prev window to " << prev_window;
    }
}

// ============================================================================
// PipelineConfigFactory Implementation
// ============================================================================

StageConfig PipelineConfigFactory::createOrderingStageConfig() {
    StageConfig config;
    config.stage_name = "Ordering";
    
    // 排序阶段主要依赖网络通信
    config.resources[ResourceType::NETWORK] = ResourceConfig(0.1, 0.9, 0.8);
    config.resources[ResourceType::CPU] = ResourceConfig(0.1, 0.9, 0.2);
    
    config.initial_window_size = 1;
    config.max_window_size = 50;
    config.min_window_size = 1;
    
    config.low_utilization_threshold = 0.3;
    config.high_utilization_threshold = 0.75;
    config.low_backlog_threshold = 2;
    config.high_backlog_threshold = 10;
    
    config.k_alpha = 0.5;
    config.k_beta = 0.3;
    
    return config;
}

StageConfig PipelineConfigFactory::createNonConflictingExecutionConfig() {
    StageConfig config;
    config.stage_name = "NonConflictingExecution";
    
    // 无冲突执行阶段高度依赖CPU
    config.resources[ResourceType::CPU] = ResourceConfig(0.2, 0.95, 0.9);
    config.resources[ResourceType::MEMORY] = ResourceConfig(0.1, 0.9, 0.1);
    
    config.initial_window_size = 1;
    config.max_window_size = 100;
    config.min_window_size = 1;
    
    config.low_utilization_threshold = 0.4;
    config.high_utilization_threshold = 0.85;
    config.low_backlog_threshold = 2;
    config.high_backlog_threshold = 15;
    
    config.k_alpha = 0.6;
    config.k_beta = 0.4;
    
    return config;
}

StageConfig PipelineConfigFactory::createNonConflictingCommitConfig() {
    StageConfig config;
    config.stage_name = "NonConflictingCommit";
    
    // 无冲突提交阶段以磁盘I/O为主
    config.resources[ResourceType::DISK_IO] = ResourceConfig(0.1, 0.9, 0.85);
    config.resources[ResourceType::CPU] = ResourceConfig(0.1, 0.9, 0.15);
    
    config.initial_window_size = 1;
    config.max_window_size = 80;
    config.min_window_size = 1;
    
    config.low_utilization_threshold = 0.3;
    config.high_utilization_threshold = 0.8;
    config.low_backlog_threshold = 3;
    config.high_backlog_threshold = 12;
    
    config.k_alpha = 0.5;
    config.k_beta = 0.35;
    
    return config;
}

StageConfig PipelineConfigFactory::createCheckpointConfig() {
    StageConfig config;
    config.stage_name = "Checkpoint";
    
    // 检查点阶段需要频繁的网络交互和存储同步
    config.resources[ResourceType::NETWORK] = ResourceConfig(0.1, 0.9, 0.5);
    config.resources[ResourceType::DISK_IO] = ResourceConfig(0.1, 0.9, 0.4);
    config.resources[ResourceType::CPU] = ResourceConfig(0.1, 0.9, 0.1);
    
    config.initial_window_size = 1;
    config.max_window_size = 60;
    config.min_window_size = 1;
    
    config.low_utilization_threshold = 0.3;
    config.high_utilization_threshold = 0.75;
    config.low_backlog_threshold = 2;
    config.high_backlog_threshold = 10;
    
    config.k_alpha = 0.4;
    config.k_beta = 0.3;
    
    return config;
}

StageConfig PipelineConfigFactory::createConflictingExecutionCommitConfig() {
    StageConfig config;
    config.stage_name = "ConflictingExecutionCommit";
    
    // 冲突执行和提交阶段涉及CPU、内存和I/O
    config.resources[ResourceType::CPU] = ResourceConfig(0.2, 0.95, 0.5);
    config.resources[ResourceType::MEMORY] = ResourceConfig(0.2, 0.9, 0.3);
    config.resources[ResourceType::DISK_IO] = ResourceConfig(0.1, 0.9, 0.2);
    
    config.initial_window_size = 1;
    config.max_window_size = 100;
    config.min_window_size = 1;
    
    config.low_utilization_threshold = 0.35;
    config.high_utilization_threshold = 0.8;
    config.low_backlog_threshold = 2;
    config.high_backlog_threshold = 12;
    
    config.k_alpha = 0.5;
    config.k_beta = 0.35;
    
    return config;
}

std::vector<StageConfig> PipelineConfigFactory::createDefaultPipelineConfig() {
    std::vector<StageConfig> configs;
    
    configs.push_back(createOrderingStageConfig());
    configs.push_back(createNonConflictingExecutionConfig());
    configs.push_back(createNonConflictingCommitConfig());
    configs.push_back(createCheckpointConfig());
    configs.push_back(createConflictingExecutionCommitConfig());
    
    return configs;
}

} // namespace pipeline
