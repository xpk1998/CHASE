#include "two_zone_types.h"
#include <algorithm>
#include <numeric>
#include <glog/logging.h>

namespace scheduling {

TwoZoneStatistics::TwoZoneStatistics()
    : has_chain_stats(false), has_shard_stats(false),
      total_execution_time_us(0), total_throughput_tps(0.0) {
}

void TwoZoneStatistics::recordNonConflictingZone(size_t tx_count, size_t sequence_count, 
                                               const std::vector<double>& parallelism, 
                                               uint64_t execution_time_us) {
    nc_zone_stats.tx_count = tx_count;
    nc_zone_stats.sequence_count = sequence_count;
    nc_zone_stats.execution_time_us = execution_time_us;
    
    if (!parallelism.empty()) {
        nc_zone_stats.avg_parallelism = std::accumulate(parallelism.begin(), parallelism.end(), 0.0) / parallelism.size();
        nc_zone_stats.max_parallelism = *std::max_element(parallelism.begin(), parallelism.end());
    } else {
        nc_zone_stats.avg_parallelism = 0.0;
        nc_zone_stats.max_parallelism = 0.0;
    }
}

void TwoZoneStatistics::recordConflictingZone(size_t tx_count, size_t sequence_count, 
                                            const std::vector<size_t>& sequence_sizes, 
                                            uint64_t execution_time_us) {
    c_zone_stats.tx_count = tx_count;
    c_zone_stats.sequence_count = sequence_count;
    c_zone_stats.execution_time_us = execution_time_us;
    
    if (!sequence_sizes.empty()) {
        c_zone_stats.avg_sequence_size = std::accumulate(sequence_sizes.begin(), sequence_sizes.end(), 0.0) / sequence_sizes.size();
        c_zone_stats.max_sequence_size = *std::max_element(sequence_sizes.begin(), sequence_sizes.end());
    } else {
        c_zone_stats.avg_sequence_size = 0.0;
        c_zone_stats.max_sequence_size = 0;
    }
}

void TwoZoneStatistics::recordValidation(size_t success_count, size_t failure_count) {
    validation_stats.success_count = success_count;
    validation_stats.failure_count = failure_count;
    
    size_t total = success_count + failure_count;
    if (total > 0) {
        validation_stats.success_rate = static_cast<double>(success_count) / total;
    } else {
        validation_stats.success_rate = 0.0;
    }
}

void TwoZoneStatistics::recordChains(size_t total_chains, size_t nc_zone_chains, size_t c_zone_chains,
                                   size_t sequences_with_chains, const std::vector<size_t>& chains_per_sequence,
                                   const std::vector<size_t>& chain_lengths) {
    chain_stats.total_chains = total_chains;
    chain_stats.num_nc_zone_chains = nc_zone_chains;
    chain_stats.num_c_zone_chains = c_zone_chains;
    chain_stats.num_sequences_with_chains = sequences_with_chains;
    
    if (total_chains > 0) {
        chain_stats.nc_zone_chain_ratio = static_cast<double>(nc_zone_chains) / total_chains;
        chain_stats.c_zone_chain_ratio = static_cast<double>(c_zone_chains) / total_chains;
    } else {
        chain_stats.nc_zone_chain_ratio = 0.0;
        chain_stats.c_zone_chain_ratio = 0.0;
    }
    
    if (!chains_per_sequence.empty()) {
        chain_stats.avg_chains_per_sequence = std::accumulate(chains_per_sequence.begin(), chains_per_sequence.end(), 0.0) / chains_per_sequence.size();
        chain_stats.max_chains_per_sequence = *std::max_element(chains_per_sequence.begin(), chains_per_sequence.end());
    } else {
        chain_stats.avg_chains_per_sequence = 0.0;
        chain_stats.max_chains_per_sequence = 0;
    }
    
    if (!chain_lengths.empty()) {
        chain_stats.avg_chain_length = std::accumulate(chain_lengths.begin(), chain_lengths.end(), 0.0) / chain_lengths.size();
        chain_stats.min_chain_length = *std::min_element(chain_lengths.begin(), chain_lengths.end());
        chain_stats.max_chain_length = *std::max_element(chain_lengths.begin(), chain_lengths.end());
    } else {
        chain_stats.avg_chain_length = 0.0;
        chain_stats.min_chain_length = 0;
        chain_stats.max_chain_length = 0;
    }
    
    has_chain_stats = true;
}

void TwoZoneStatistics::recordShards(size_t total_shard_count, size_t nc_zone_shard_count, size_t c_zone_shard_count,
                                   const std::vector<double>& nc_zone_shard_parallelism,
                                   const std::vector<double>& c_zone_shard_parallelism) {
    shard_stats.total_shard_count = total_shard_count;
    shard_stats.nc_zone_shard_count = nc_zone_shard_count;
    shard_stats.c_zone_shard_count = c_zone_shard_count;
    
    if (!nc_zone_shard_parallelism.empty()) {
        shard_stats.nc_zone_avg_shard_parallelism = std::accumulate(nc_zone_shard_parallelism.begin(), nc_zone_shard_parallelism.end(), 0.0) / nc_zone_shard_parallelism.size();
        shard_stats.nc_zone_max_shard_parallelism = *std::max_element(nc_zone_shard_parallelism.begin(), nc_zone_shard_parallelism.end());
    } else {
        shard_stats.nc_zone_avg_shard_parallelism = 0.0;
        shard_stats.nc_zone_max_shard_parallelism = 0.0;
    }
    
    if (!c_zone_shard_parallelism.empty()) {
        shard_stats.c_zone_avg_shard_parallelism = std::accumulate(c_zone_shard_parallelism.begin(), c_zone_shard_parallelism.end(), 0.0) / c_zone_shard_parallelism.size();
        shard_stats.c_zone_max_shard_parallelism = *std::max_element(c_zone_shard_parallelism.begin(), c_zone_shard_parallelism.end());
    } else {
        shard_stats.c_zone_avg_shard_parallelism = 0.0;
        shard_stats.c_zone_max_shard_parallelism = 0.0;
    }
    
    size_t total_shards = nc_zone_shard_count + c_zone_shard_count;
    if (total_shards > 0) {
        double total_parallelism = 
            (shard_stats.nc_zone_avg_shard_parallelism * nc_zone_shard_count +
             shard_stats.c_zone_avg_shard_parallelism * c_zone_shard_count);
        shard_stats.overall_avg_shard_parallelism = total_parallelism / total_shards;
    } else {
        shard_stats.overall_avg_shard_parallelism = 0.0;
    }
    
    has_shard_stats = true;
}

void TwoZoneStatistics::calculateOverallStats() {
    size_t total_tx = nc_zone_stats.tx_count + c_zone_stats.tx_count;
    if (total_execution_time_us > 0) {
        total_throughput_tps = (static_cast<double>(total_tx) * 1000000.0) / total_execution_time_us;
    } else {
        total_throughput_tps = 0.0;
    }
}

void TwoZoneStatistics::printStats() const {
    LOG(INFO) << "=== Two-Zone Execution Statistics ===";
    
    // Non-conflicting zone statistics
    LOG(INFO) << "Non-Conflicting Zone:";
    LOG(INFO) << "  Transactions: " << nc_zone_stats.tx_count;
    LOG(INFO) << "  Sequences: " << nc_zone_stats.sequence_count;
    LOG(INFO) << "  Avg Parallelism: " << nc_zone_stats.avg_parallelism;
    LOG(INFO) << "  Max Parallelism: " << nc_zone_stats.max_parallelism;
    LOG(INFO) << "  Execution Time: " << nc_zone_stats.execution_time_us << " us";
    
    // Conflicting zone statistics
    LOG(INFO) << "Conflicting Zone:";
    LOG(INFO) << "  Transactions: " << c_zone_stats.tx_count;
    LOG(INFO) << "  Sequences: " << c_zone_stats.sequence_count;
    LOG(INFO) << "  Avg Sequence Size: " << c_zone_stats.avg_sequence_size;
    LOG(INFO) << "  Max Sequence Size: " << c_zone_stats.max_sequence_size;
    LOG(INFO) << "  Execution Time: " << c_zone_stats.execution_time_us << " us";
    
    // Validation statistics
    LOG(INFO) << "Optimistic Validation:";
    LOG(INFO) << "  Valid Transactions: " << validation_stats.success_count;
    LOG(INFO) << "  Invalid Transactions: " << validation_stats.failure_count;
    LOG(INFO) << "  Success Rate: " << (validation_stats.success_rate * 100.0) << "%";
    
    // Dependency chain statistics (Task 10.3)
    // Requirement 5.4: Calculate chain-based metrics
    if (has_chain_stats) {
        LOG(INFO) << "Dependency Chain Statistics:";
        LOG(INFO) << "  Total Chains: " << chain_stats.total_chains;
        LOG(INFO) << "  Non-Conflicting Zone Chains: " << chain_stats.num_nc_zone_chains
                  << " (" << (chain_stats.nc_zone_chain_ratio * 100.0) << "%)";
        LOG(INFO) << "  Conflicting Zone Chains: " << chain_stats.num_c_zone_chains
                  << " (" << (chain_stats.c_zone_chain_ratio * 100.0) << "%)";
        LOG(INFO) << "  Sequences with Chains: " << chain_stats.num_sequences_with_chains;
        LOG(INFO) << "  Avg Chains per Sequence: " << chain_stats.avg_chains_per_sequence;
        LOG(INFO) << "  Max Chains per Sequence: " << chain_stats.max_chains_per_sequence;
        LOG(INFO) << "  Avg Chain Length: " << chain_stats.avg_chain_length;
        LOG(INFO) << "  Chain Length Range: [" << chain_stats.min_chain_length
                  << ", " << chain_stats.max_chain_length << "]";
    }
    
    // Sharding statistics (Task 11.3)
    // Requirement 7.5: Record shard count per zone, shard-level parallelism, and calculate shard-based metrics
    if (has_shard_stats) {
        LOG(INFO) << "Sharding Statistics:";
        LOG(INFO) << "  Total Shards: " << shard_stats.total_shard_count;
        LOG(INFO) << "  Non-Conflicting Zone:";
        LOG(INFO) << "    Shard Count: " << shard_stats.nc_zone_shard_count;
        LOG(INFO) << "    Avg Shard Parallelism: " << shard_stats.nc_zone_avg_shard_parallelism;
        LOG(INFO) << "    Max Shard Parallelism: " << shard_stats.nc_zone_max_shard_parallelism;
        LOG(INFO) << "  Conflicting Zone:";
        LOG(INFO) << "    Shard Count: " << shard_stats.c_zone_shard_count;
        LOG(INFO) << "    Avg Shard Parallelism: " << shard_stats.c_zone_avg_shard_parallelism;
        LOG(INFO) << "    Max Shard Parallelism: " << shard_stats.c_zone_max_shard_parallelism;
        LOG(INFO) << "  Overall Avg Shard Parallelism: " << shard_stats.overall_avg_shard_parallelism;
    }
    
    // Overall statistics
    LOG(INFO) << "Overall:";
    LOG(INFO) << "  Total Transactions: " 
              << (nc_zone_stats.tx_count + c_zone_stats.tx_count);
    LOG(INFO) << "  Total Execution Time: " << total_execution_time_us << " us";
    LOG(INFO) << "  Throughput: " << total_throughput_tps << " TPS";
    
    // Zone distribution
    size_t total_tx = nc_zone_stats.tx_count + c_zone_stats.tx_count;
    if (total_tx > 0) {
        double nc_ratio = static_cast<double>(nc_zone_stats.tx_count) / total_tx * 100.0;
        double c_ratio = static_cast<double>(c_zone_stats.tx_count) / total_tx * 100.0;
        LOG(INFO) << "Zone Distribution:";
        LOG(INFO) << "  Non-Conflicting: " << nc_ratio << "%";
        LOG(INFO) << "  Conflicting: " << c_ratio << "%";
    }
    
    // Task 12.3: Performance metrics
    // Requirement 8.6: Log throughput (transactions per second)
    // Requirement 8.6: Log speedup vs baseline
    // Requirement 8.6: Log resource utilization
    LOG(INFO) << "Performance Metrics:";
    
    // Calculate speedup vs sequential baseline
    // Baseline: all transactions executed sequentially
    // Estimated sequential time = total_tx * avg_tx_time
    // Speedup = sequential_time / parallel_time
    if (total_execution_time_us > 0 && total_tx > 0) {
        // Estimate average transaction time from non-conflicting zone
        // (assuming NC zone shows true parallel performance)
        double avg_tx_time_us = 0.0;
        if (nc_zone_stats.tx_count > 0 && nc_zone_stats.execution_time_us > 0) {
            avg_tx_time_us = static_cast<double>(nc_zone_stats.execution_time_us) / 
                            nc_zone_stats.tx_count;
        } else if (c_zone_stats.tx_count > 0 && c_zone_stats.execution_time_us > 0) {
            avg_tx_time_us = static_cast<double>(c_zone_stats.execution_time_us) / 
                            c_zone_stats.tx_count;
        }
        
        if (avg_tx_time_us > 0) {
            double estimated_sequential_time_us = total_tx * avg_tx_time_us;
            double speedup = estimated_sequential_time_us / total_execution_time_us;
            
            LOG(INFO) << "  Estimated Sequential Time: " << estimated_sequential_time_us << " us";
            LOG(INFO) << "  Speedup vs Sequential: " << speedup << "x";
            LOG(INFO) << "  Parallel Efficiency: " 
                      << (speedup / (nc_zone_stats.max_parallelism > 0 ? 
                          nc_zone_stats.max_parallelism : 1.0) * 100.0) << "%";
        }
        
        // Resource utilization metrics
        LOG(INFO) << "Resource Utilization:";
        
        // NC zone utilization (based on parallelism)
        if (nc_zone_stats.max_parallelism > 0) {
            double nc_utilization = (nc_zone_stats.avg_parallelism / 
                                    nc_zone_stats.max_parallelism) * 100.0;
            LOG(INFO) << "  NC Zone CPU Utilization: " << nc_utilization << "%";
            LOG(INFO) << "  NC Zone Avg Concurrent Tx: " << nc_zone_stats.avg_parallelism;
            LOG(INFO) << "  NC Zone Max Concurrent Tx: " << nc_zone_stats.max_parallelism;
        }
        
        // Time distribution
        double nc_time_pct = (static_cast<double>(nc_zone_stats.execution_time_us) / 
                             total_execution_time_us) * 100.0;
        double c_time_pct = (static_cast<double>(c_zone_stats.execution_time_us) / 
                            total_execution_time_us) * 100.0;
        LOG(INFO) << "  Time in NC Zone: " << nc_time_pct << "%";
        LOG(INFO) << "  Time in C Zone: " << c_time_pct << "%";
        
        // Transaction processing rate
        LOG(INFO) << "  NC Zone Rate: " 
                  << (nc_zone_stats.execution_time_us > 0 ?
                      (nc_zone_stats.tx_count * 1000000.0 / nc_zone_stats.execution_time_us) : 0.0)
                  << " tx/s";
        LOG(INFO) << "  C Zone Rate: " 
                  << (c_zone_stats.execution_time_us > 0 ?
                      (c_zone_stats.tx_count * 1000000.0 / c_zone_stats.execution_time_us) : 0.0)
                  << " tx/s";
    }
    
    LOG(INFO) << "=====================================";
}

} // namespace scheduling