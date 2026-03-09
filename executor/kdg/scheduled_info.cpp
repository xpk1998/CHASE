#include "address_based_conflict_graph.h"
#include "glog/logging.h"
#include <algorithm>
#include <cmath>

namespace scheduling {

// Get parallelism metrics
ScheduledInfo::ParallelismMetrics ScheduledInfo::getParallelismMetrics() const {
    ParallelismMetrics metrics;
    metrics.total_tx = scheduledTxsCount();
    
    if (non_conflicting_zone_txs.empty()) {
        metrics.average_width = 0.0;
        metrics.std_width = 0.0;
        metrics.max_width = 0;
        metrics.depth = 0;
        return metrics;
    }
    
    // Calculate width statistics
    std::vector<size_t> widths;
    widths.reserve(non_conflicting_zone_txs.size());
    
    for (const auto& group : non_conflicting_zone_txs) {
        widths.push_back(group.size());
    }
    
    // Calculate average width
    size_t total_width = 0;
    for (size_t width : widths) {
        total_width += width;
    }
    metrics.average_width = static_cast<double>(total_width) / widths.size();
    
    // Calculate standard deviation
    double sum_sq_diff = 0.0;
    for (size_t width : widths) {
        double diff = static_cast<double>(width) - metrics.average_width;
        sum_sq_diff += diff * diff;
    }
    metrics.std_width = std::sqrt(sum_sq_diff / widths.size());
    
    // Find max width
    metrics.max_width = *std::max_element(widths.begin(), widths.end());
    
    // Depth is the number of sequence layers
    metrics.depth = widths.size();
    
    return metrics;
}

// Create from transaction lists
ScheduledInfo ScheduledInfo::create(
    std::unordered_map<uint64_t, std::shared_ptr<TransactionNode>>& tx_list,
    std::vector<std::shared_ptr<TransactionNode>>& aborted_txs) {
    ScheduledInfo info;
    
    // Simple implementation: put all non-aborted transactions in non-conflicting zone
    // and aborted transactions in conflicting zone
    
    std::vector<FinalizedTransaction> non_conflicting_txs;
    std::vector<FinalizedTransaction> conflicting_txs;
    
    for (const auto& pair : tx_list) {
        const auto& tx_node = pair.second;
        if (!tx_node->aborted()) {
            // Add to non-conflicting zone
            FinalizedTransaction finalized_tx(
                tx_node->id(),
                tx_node->sequence(),
                tx_node->rawTransaction(),
                tx_node->readKeys(),
                tx_node->writeKeys()
            );
            non_conflicting_txs.push_back(finalized_tx);
        }
    }
    
    // Group non-conflicting transactions by sequence number
    std::map<uint32_t, std::vector<FinalizedTransaction>> non_conflicting_groups;
    for (const auto& tx : non_conflicting_txs) {
        non_conflicting_groups[tx.sequence].push_back(tx);
    }
    
    // Convert to vectors
    for (const auto& pair : non_conflicting_groups) {
        info.non_conflicting_zone_txs.push_back(pair.second);
    }
    
    // Handle aborted transactions
    for (const auto& tx_node : aborted_txs) {
        AbortedTransaction aborted_tx(tx_node);
        info.aborted_txs.push_back(aborted_tx);
    }
    
    return info;
}

// Create from transaction lists with dependency chains
ScheduledInfo ScheduledInfo::createWithDependencyChains(
    std::unordered_map<uint64_t, std::shared_ptr<TransactionNode>>& tx_list,
    std::vector<std::shared_ptr<TransactionNode>>& aborted_txs,
    DependencyChainResult&& chain_result) {
    ScheduledInfo info = create(tx_list, aborted_txs);
    
    // Add dependency chain information
    info.has_dependency_chains = true;
    info.dependency_chains = std::make_shared<DependencyChainResult>(std::move(chain_result));
    
    return info;
}

// Find minimum sequence in conflicting zone with no conflicts
size_t ScheduledInfo::findMinimumSequenceWithNoConflicts(
    const std::set<std::string>& read_keys,
    const std::set<std::string>& write_keys,
    const std::vector<std::set<std::string>>& sequence_map) {
    // Simple implementation: find first sequence with no conflicts
    for (size_t i = 0; i < sequence_map.size(); ++i) {
        const std::set<std::string>& existing_keys = sequence_map[i];
        
        // Check for conflicts
        bool has_conflict = false;
        for (const std::string& read_key : read_keys) {
            if (existing_keys.find(read_key) != existing_keys.end()) {
                has_conflict = true;
                break;
            }
        }
        
        if (!has_conflict) {
            for (const std::string& write_key : write_keys) {
                if (existing_keys.find(write_key) != existing_keys.end()) {
                    has_conflict = true;
                    break;
                }
            }
        }
        
        if (!has_conflict) {
            return i;
        }
    }
    
    // If no conflict-free sequence found, return next sequence
    return sequence_map.size();
}

// Schedule sorted transactions
std::vector<std::vector<FinalizedTransaction>> ScheduledInfo::scheduleSortedTxs(
    std::unordered_map<uint64_t, std::shared_ptr<TransactionNode>>& tx_list) {
    // Simple implementation: group by sequence number
    std::map<uint32_t, std::vector<FinalizedTransaction>> groups;
    
    for (const auto& pair : tx_list) {
        const auto& tx_node = pair.second;
        if (!tx_node->aborted()) {
            FinalizedTransaction finalized_tx(
                tx_node->id(),
                tx_node->sequence(),
                tx_node->rawTransaction(),
                tx_node->readKeys(),
                tx_node->writeKeys()
            );
            groups[tx_node->sequence()].push_back(finalized_tx);
        }
    }
    
    // Convert to vector of vectors
    std::vector<std::vector<FinalizedTransaction>> result;
    for (const auto& pair : groups) {
        result.push_back(pair.second);
    }
    
    return result;
}

// Schedule aborted transactions for conflicting zone sequential reordering
std::vector<AbortedTransaction> ScheduledInfo::scheduleAbortedTxs(
    std::vector<std::shared_ptr<TransactionNode>>& aborted_txs) {
    std::vector<AbortedTransaction> result;
    result.reserve(aborted_txs.size());
    
    for (const auto& tx_node : aborted_txs) {
        AbortedTransaction aborted_tx(tx_node);
        result.push_back(aborted_tx);
    }
    
    return result;
}

} // namespace scheduling