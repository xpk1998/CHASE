#ifndef NEUBLOCKCHAIN_TWO_ZONE_TYPES_H
#define NEUBLOCKCHAIN_TWO_ZONE_TYPES_H

#include <unordered_set>
#include <string>
#include <vector>

namespace scheduling {

// Type alias for hash set
template<typename T>
using HashSet = std::unordered_set<T>;

// Transaction ID type
using tid_size_t = uint64_t;

// Epoch size type
using epoch_size_t = uint64_t;

// Block height type
using block_height_t = uint64_t;

// Non-conflicting zone statistics
struct NonConflictingZoneStats {
    size_t tx_count;
    size_t sequence_count;
    double avg_parallelism;
    double max_parallelism;
    uint64_t execution_time_us;
    
    NonConflictingZoneStats() 
        : tx_count(0), sequence_count(0), avg_parallelism(0.0), 
          max_parallelism(0.0), execution_time_us(0) {}
};

// Conflicting zone statistics
struct ConflictingZoneStats {
    size_t tx_count;
    size_t sequence_count;
    double avg_sequence_size;
    size_t max_sequence_size;
    uint64_t execution_time_us;
    
    ConflictingZoneStats() 
        : tx_count(0), sequence_count(0), avg_sequence_size(0.0), 
          max_sequence_size(0), execution_time_us(0) {}
};

// Validation statistics
struct ValidationStats {
    size_t success_count;
    size_t failure_count;
    double success_rate;
    
    ValidationStats() 
        : success_count(0), failure_count(0), success_rate(0.0) {}
};

// Chain statistics
struct ChainStats {
    size_t total_chains;
    size_t num_nc_zone_chains;
    size_t num_c_zone_chains;
    double nc_zone_chain_ratio;
    double c_zone_chain_ratio;
    size_t num_sequences_with_chains;
    double avg_chains_per_sequence;
    size_t max_chains_per_sequence;
    double avg_chain_length;
    size_t min_chain_length;
    size_t max_chain_length;
    
    ChainStats() 
        : total_chains(0), num_nc_zone_chains(0), num_c_zone_chains(0),
          nc_zone_chain_ratio(0.0), c_zone_chain_ratio(0.0),
          num_sequences_with_chains(0), avg_chains_per_sequence(0.0),
          max_chains_per_sequence(0), avg_chain_length(0.0),
          min_chain_length(0), max_chain_length(0) {}
};

// Shard statistics
struct ShardStats {
    size_t total_shard_count;
    size_t nc_zone_shard_count;
    size_t c_zone_shard_count;
    double nc_zone_avg_shard_parallelism;
    double nc_zone_max_shard_parallelism;
    double c_zone_avg_shard_parallelism;
    double c_zone_max_shard_parallelism;
    double overall_avg_shard_parallelism;
    
    ShardStats() 
        : total_shard_count(0), nc_zone_shard_count(0), c_zone_shard_count(0),
          nc_zone_avg_shard_parallelism(0.0), nc_zone_max_shard_parallelism(0.0),
          c_zone_avg_shard_parallelism(0.0), c_zone_max_shard_parallelism(0.0),
          overall_avg_shard_parallelism(0.0) {}
};

// Two-zone execution statistics
struct TwoZoneStatistics {
    // Non-conflicting zone statistics
    NonConflictingZoneStats nc_zone_stats;
    
    // Conflicting zone statistics
    ConflictingZoneStats c_zone_stats;
    
    // Validation statistics
    ValidationStats validation_stats;
    
    // Chain statistics
    ChainStats chain_stats;
    bool has_chain_stats;
    
    // Shard statistics
    ShardStats shard_stats;
    bool has_shard_stats;
    
    // Overall statistics
    uint64_t total_execution_time_us;
    double total_throughput_tps;
    
    // Constructor
    TwoZoneStatistics();
    
    // Methods
    void recordNonConflictingZone(size_t tx_count, size_t sequence_count, 
                                const std::vector<double>& parallelism, uint64_t execution_time_us);
    void recordConflictingZone(size_t tx_count, size_t sequence_count, 
                             const std::vector<size_t>& sequence_sizes, uint64_t execution_time_us);
    void recordValidation(size_t success_count, size_t failure_count);
    void recordChains(size_t total_chains, size_t nc_zone_chains, size_t c_zone_chains,
                     size_t sequences_with_chains, const std::vector<size_t>& chains_per_sequence,
                     const std::vector<size_t>& chain_lengths);
    void recordShards(size_t total_shard_count, size_t nc_zone_shard_count, size_t c_zone_shard_count,
                     const std::vector<double>& nc_zone_shard_parallelism,
                     const std::vector<double>& c_zone_shard_parallelism);
    void calculateOverallStats();
    void printStats() const;
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_TWO_ZONE_TYPES_H