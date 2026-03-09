//
// Created for New Scheduling Scheme - Dependency Chain Sharding
// ShardOptimizer: Iteratively optimizes shard load balance
//

#ifndef NEUBLOCKCHAIN_SHARD_OPTIMIZER_H
#define NEUBLOCKCHAIN_SHARD_OPTIMIZER_H

#include <vector>
#include "shard.h"
//#include "dependency_chain.h"
#include "shard_manager.h"  // 包含shard_manager.h以使用OptimizationResult

namespace scheduling {

/**
 * ShardOptimizer: Iteratively optimizes shard load balance
 * 
 * ========================================================================
 * OPTIMIZATION OBJECTIVE
 * ========================================================================
 * 
 * Minimize load standard deviation D across all k shards:
 * 
 *     D = sqrt( (1/k) * Σ_{i=1}^k (Load(S_i) - Load_avg)^2 )
 * 
 *     where Load_avg = (1/k) * Σ_{i=1}^k Load(S_i)
 * 
 * Lower D ⇒ Better load balance ⇒ Better parallelization efficiency
 * 
 * ========================================================================
 * OPTIMIZATION OPERATIONS
 * ========================================================================
 * 
 * 1. CHAIN MIGRATION (Preferred when load difference is large)
 * 
 *    Condition: |Load(S_p) - Load(S_q)| > 2 * Load(L_a)
 * 
 *    Operation:
 *    - Move chain L_a from high-load shard S_p to low-load shard S_q
 *    - Load'(S_p) = Load(S_p) - Load(L_a)
 *    - Load'(S_q) = Load(S_q) + Load(L_a)
 * 
 *    Selection Strategy:
 *    - Choose L_a with load closest to (Load(S_p) - Load(S_q)) / 2
 *    - Aims for balanced redistribution
 * 
 * 2. CHAIN SWAP (Used when load difference is moderate)
 * 
 *    Condition: |Load(S_p) - Load(S_q)| ≤ 2 * Load(L_a)
 * 
 *    Operation:
 *    - Exchange chains L_a ∈ S_p and L_b ∈ S_q
 *    - Constraint: L_a and L_b must have SAME conflict attribute
 *                  (maintains zone segregation)
 *    - Load'(S_p) = Load(S_p) - Load(L_a) + Load(L_b)
 *    - Load'(S_q) = Load(S_q) - Load(L_b) + Load(L_a)
 * 
 *    Selection Strategy:
 *    - Choose pair (L_a, L_b) that minimizes new load difference
 *    - Ensures maximum balance improvement
 * 
 * ========================================================================
 * ITERATIVE ALGORITHM
 * ========================================================================
 * 
 * 1. Calculate initial standard deviation D
 * 
 * 2. Loop (until convergence or max iterations):
 *    a) Select shard pair:
 *       - S_p = shard with maximum load
 *       - S_q = shard with minimum load
 *    
 *    b) Try migration or swap (based on condition above)
 *    
 *    c) Calculate new standard deviation D':
 *       Optimization: Only S_p and S_q loads change (TxAllo)
 *       
 *       D' = sqrt( (1/k) * [ (Load'(S_p) - Load_avg)^2 +
 *                            (Load'(S_q) - Load_avg)^2 +
 *                            Σ_{i∉{p,q}} (Load(S_i) - Load_avg)^2 ] )
 *    
 *    d) Evaluate benefit:
 *       if D - D' > ε:  # Improvement exceeds threshold
 *         Execute operation
 *         Update D = D'
 *       else:
 *         TERMINATE (converged)
 * 
 * 3. Return optimization statistics
 * 
 * ========================================================================
 * CONVERGENCE CONDITIONS
 * ========================================================================
 * 
 * - Benefit threshold not met: D - D' ≤ ε
 * - Maximum iterations reached
 * - No valid operations available (equal loads)
 * 
 * ========================================================================
 * CONSTRAINTS
 * ========================================================================
 * 
 * - Zone segregation must be maintained:
 *   · NC chains stay in NC zones
 *   · C chains stay in C zones
 *   · Swap operations only between chains of same type
 * 
 * - Chain atomicity:
 *   · Chains are moved/swapped as whole units
 *   · No cross-shard splitting
 * 
 * ========================================================================
 */
class ShardOptimizer {
public:
    explicit ShardOptimizer(std::vector<Shard>& shards,
                           double threshold,
                           uint32_t max_iterations);
    
    // Perform optimization
    // Requirements 11.1-11.5: Iterative optimization with termination conditions
    OptimizationResult optimize();

private:
    // Select shard pair with maximum load difference
    // Requirement 8.1, 8.2, 8.3: Select Sp (max load) and Sq (min load)
    std::pair<uint32_t, uint32_t> selectShardPair();
    
    // Enhanced chain migration with better candidate selection
    // Requirements 9.1-9.5: Migrate chain from Sp to Sq
    // Condition: |Load(Sp) - Load(Sq)| > 2 * Load(La)
    bool tryEnhancedChainMigration(uint32_t shard_p_id, uint32_t shard_q_id, double current_std_dev);
    
    // Enhanced chain swap with better candidate selection
    // Requirements 10.1-10.6: Swap chains between Sp and Sq
    // Condition: |Load(Sp) - Load(Sq)| <= 2 * Load(La)
    bool tryEnhancedChainSwap(uint32_t shard_p_id, uint32_t shard_q_id, double current_std_dev);
    
    // Evaluate benefit
    // Requirement 9.4, 10.5, 11.3: Check if D - D' > ε
    bool evaluateBenefit(double current_std_dev, double new_std_dev);
    
    // Enhanced selection of migration candidate from high-load shard
    // Requirement 9.2: Select candidate chain La from Sp
    // Strategy: Select chain with load closest to (Load(Sp) - Load(Sq)) / 2
    // Enhancement: Consider multiple candidates and select the best one
    DependencyChain* selectEnhancedMigrationCandidate(Shard& shard_p, const Shard& shard_q);
    
    // Enhanced selection of swap candidates from both shards
    // Requirement 10.2, 10.3: Select La from Sp and Lb from Sq
    // Constraint: La and Lb must have same conflict attribute
    // Enhancement: Consider multiple pairs and select the best one
    std::pair<DependencyChain*, DependencyChain*> 
        selectEnhancedSwapCandidates(Shard& shard_p, Shard& shard_q);
    
    // Execute chain migration
    // Requirement 9.4, 9.5: Move La from Sp to Sq and update loads
    void executeMigration(Shard& shard_p, Shard& shard_q, DependencyChain& chain);
    
    // Execute chain swap
    // Requirement 10.5, 10.6: Swap La and Lb between Sp and Sq and update loads
    void executeSwap(Shard& shard_p, Shard& shard_q, 
                    DependencyChain& chain_a, DependencyChain& chain_b);
    
    // Calculate load balance improvement for a potential migration
    double calculateMigrationImprovement(Shard& shard_p, Shard& shard_q, DependencyChain& chain);
    
    // Calculate load balance improvement for a potential swap
    double calculateSwapImprovement(Shard& shard_p, Shard& shard_q, 
                                  DependencyChain& chain_a, DependencyChain& chain_b);
    
    std::vector<Shard>& shards_;
    double threshold_;           // Optimization threshold (ε)
    uint32_t max_iterations_;    // Maximum iterations
    uint32_t iteration_count_;   // Current iteration count
    uint32_t num_migrations_;    // Number of migrations performed
    uint32_t num_swaps_;         // Number of swaps performed
};

}  // namespace scheduling

#endif  // NEUBLOCKCHAIN_SHARD_OPTIMIZER_H