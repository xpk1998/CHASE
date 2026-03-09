#ifndef NEUBLOCKCHAIN_DEPENDENCY_CHAIN_EXTRACTOR_H
#define NEUBLOCKCHAIN_DEPENDENCY_CHAIN_EXTRACTOR_H

#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include "kdg_node.h"
#include "dependency_chain.h"
// #include "union_find_chain_manager.h"  // File not found

namespace scheduling {

// Forward declarations
class DependencyDetector;
class ChainLoadCalculator;

// DependencyChainExtractor: Extracts dependency chains from KDG
// Implements the dependency chain extraction algorithm for the BLP scheduler
class DependencyChainExtractor {
public:
    // Constructor
    explicit DependencyChainExtractor(bool enable_parallel = true, 
                                     int num_threads = 0);
    
    // Extract all dependency chains from transaction list and aborted transactions
    // Requirements: 1.1-1.5, 2.1-2.5, 6.1-6.5, 8.1-8.5
    DependencyChainResult extractAllChains(
        const std::unordered_map<uint64_t, std::shared_ptr<TransactionNode>>& tx_list,
        const std::vector<std::shared_ptr<TransactionNode>>& aborted_txs);

private:
    // Group transactions by sequence number
    // Input: Transaction list from KDG
    // Output: Map of sequence_number -> [transactions]
    // Requirements: 1.1, 1.2
    std::map<uint32_t, std::vector<std::shared_ptr<TransactionNode>>> 
        groupBySequence(const std::unordered_map<uint64_t, std::shared_ptr<TransactionNode>>& tx_list);
    
    // Validate sequence numbers
    // Check that sequence numbers are valid and complete
    // Requirement: 1.2
    bool validateSequenceNumbers(
        const std::map<uint32_t, std::vector<std::shared_ptr<TransactionNode>>>& grouped_txs);
    
    // Extract dependency chains from a sequence group
    // Requirements: 2.1-2.5
    std::vector<DependencyChain> extractChainsFromSequence(
        const std::vector<std::shared_ptr<TransactionNode>>& txs, uint32_t sequence);
    
    // Sequential chain extraction
    // Requirements: 2.1-2.5
    std::vector<DependencyChain> extractChainsSequential(
        const std::vector<std::shared_ptr<TransactionNode>>& txs, uint32_t sequence);
    
    // Parallel chain extraction
    // Requirements: 2.1-2.5
    std::vector<DependencyChain> extractChainsParallel(
        const std::vector<std::shared_ptr<TransactionNode>>& txs, uint32_t sequence);
    
    // Extract chains from aborted transactions
    // Requirements: 6.1-6.5
    std::vector<DependencyChain> extractChainsFromAbortedTransactions(
        const std::vector<std::shared_ptr<TransactionNode>>& aborted_txs);
    
    // Serial chain extension
    // Extend chains sequentially through sequence numbers
    void serialExtendChains(
        const std::vector<std::shared_ptr<TransactionNode>>& current_seq_txs,
        const std::vector<std::shared_ptr<TransactionNode>>& next_seq_txs
        // UnionFindChainManager& chain_manager
        );
    
    // Parallel chain extension
    // Extend chains in parallel for better performance
    // Requirements: 6.1, 6.2, 6.3, 6.4, 6.5
    void parallelExtendChains(
        const std::vector<std::shared_ptr<TransactionNode>>& current_seq_txs,
        const std::vector<std::shared_ptr<TransactionNode>>& next_seq_txs
        // UnionFindChainManager& chain_manager
        );
    
    // Build dependency chains from union-find structure
    // Convert union-find results to DependencyChain objects
    std::vector<DependencyChain> buildChainsFromUnionFind(
        const std::map<uint32_t, std::vector<std::shared_ptr<TransactionNode>>>& grouped_txs
        // UnionFindChainManager& chain_manager
        );
    
    // Calculate statistics for extracted chains
    void calculateStatistics(
        DependencyChainResult& result,
        uint64_t extraction_time_us);
    
    // Calculate chain statistics
    // Requirements: 10.1-10.5
    void calculateChainStatistics(DependencyChainResult& result);
    
    // Detect circular dependencies in chains
    // Requirement 11.3: Detect and handle circular dependencies
    bool detectCircularDependency(
        const std::vector<DependencyChain>& chains) const;
    
    // Break circular dependencies using heuristic
    // Requirement 11.3: Use heuristic method to break cycles
    void breakCircularDependencies(
        std::vector<DependencyChain>& chains);
    
    bool enable_parallel_;
    int num_threads_;
    ChainStatistics statistics_;
};

} // namespace scheduling

#endif //NEUBLOCKCHAIN_DEPENDENCY_CHAIN_EXTRACTOR_H