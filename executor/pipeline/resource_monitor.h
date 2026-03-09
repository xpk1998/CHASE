//
// Created for Block Level Pipelining Resource Monitoring
// System Resource Monitor for Pipeline Control
//

#ifndef NEUBLOCKCHAIN_RESOURCE_MONITOR_H
#define NEUBLOCKCHAIN_RESOURCE_MONITOR_H

#include <atomic>
#include <chrono>
#include <thread>
#include <memory>
#include <functional>
#include "pipeline_controller.h"

namespace pipeline {

/**
 * ResourceMonitor: 系统资源监控器
 * 
 * 功能：
 * 1. 周期性采集CPU、内存、磁盘I/O、网络带宽利用率
 * 2. 将资源数据反馈给PipelineController
 * 3. 支持自定义采样频率
 */
class ResourceMonitor {
public:
    /**
     * 构造函数
     * @param controller 流水线控制器指针
     * @param sampling_interval_ms 采样间隔（毫秒）
     */
    explicit ResourceMonitor(PipelineController* controller, 
                            uint64_t sampling_interval_ms = 1000);
    
    ~ResourceMonitor();
    
    // 禁止拷贝和移动
    ResourceMonitor(const ResourceMonitor&) = delete;
    ResourceMonitor& operator=(const ResourceMonitor&) = delete;
    
    /**
     * 启动资源监控
     */
    void start();
    
    /**
     * 停止资源监控
     */
    void stop();
    
    /**
     * 是否正在运行
     */
    bool isRunning() const { return running_.load(); }
    
    /**
     * 手动触发一次资源采样
     */
    void sampleOnce();
    
    /**
     * 设置自定义CPU采样函数
     * @param sampler 返回CPU利用率 [0.0, 1.0]
     */
    void setCPUSampler(std::function<double()> sampler) {
        cpu_sampler_ = sampler;
    }
    
    /**
     * 设置自定义内存采样函数
     * @param sampler 返回内存利用率 [0.0, 1.0]
     */
    void setMemorySampler(std::function<double()> sampler) {
        memory_sampler_ = sampler;
    }
    
    /**
     * 设置自定义磁盘I/O采样函数
     * @param sampler 返回磁盘I/O利用率 [0.0, 1.0]
     */
    void setDiskIOSampler(std::function<double()> sampler) {
        disk_io_sampler_ = sampler;
    }
    
    /**
     * 设置自定义网络采样函数
     * @param sampler 返回网络利用率 [0.0, 1.0]
     */
    void setNetworkSampler(std::function<double()> sampler) {
        network_sampler_ = sampler;
    }
    
private:
    /**
     * 监控线程主循环
     */
    void monitorLoop();
    
    /**
     * 默认CPU利用率采样
     */
    double sampleCPU();
    
    /**
     * 默认内存利用率采样
     */
    double sampleMemory();
    
    /**
     * 默认磁盘I/O利用率采样
     */
    double sampleDiskIO();
    
    /**
     * 默认网络利用率采样
     */
    double sampleNetwork();
    
    // 流水线控制器
    PipelineController* controller_;
    
    // 采样间隔
    uint64_t sampling_interval_ms_;
    
    // 运行状态
    std::atomic<bool> running_{false};
    
    // 监控线程
    std::unique_ptr<std::thread> monitor_thread_;
    
    // 自定义采样器
    std::function<double()> cpu_sampler_;
    std::function<double()> memory_sampler_;
    std::function<double()> disk_io_sampler_;
    std::function<double()> network_sampler_;
    
    // 上次I/O统计（用于计算增量）
    struct IOStats {
        uint64_t read_bytes;
        uint64_t write_bytes;
        std::chrono::steady_clock::time_point timestamp;
        
        IOStats() : read_bytes(0), write_bytes(0), 
                   timestamp(std::chrono::steady_clock::now()) {}
    };
    
    IOStats last_io_stats_;
    
    // 上次网络统计
    struct NetworkStats {
        uint64_t rx_bytes;
        uint64_t tx_bytes;
        std::chrono::steady_clock::time_point timestamp;
        
        NetworkStats() : rx_bytes(0), tx_bytes(0),
                        timestamp(std::chrono::steady_clock::now()) {}
    };
    
    NetworkStats last_network_stats_;
};

} // namespace pipeline

#endif // NEUBLOCKCHAIN_RESOURCE_MONITOR_H
