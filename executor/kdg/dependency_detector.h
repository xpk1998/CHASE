//
// Created for New Scheduling Scheme Dependency Chain Extraction
// DependencyDetector: Detects read-write dependencies between transactions
// Requirements: 4.1, 4.2, 4.3, 4.4, 4.5
//

#ifndef NEUBLOCKCHAIN_DEPENDENCY_DETECTOR_H
#define NEUBLOCKCHAIN_DEPENDENCY_DETECTOR_H

#include <memory>
#include <set>
#include <string>
#include <algorithm>
#include "kdg_node.h"

namespace scheduling {

// DependencyDetector: Static utility class for detecting dependencies between transactions
// Provides methods to check read-write dependencies and identify dependency types
class DependencyDetector {
public:
    // Dependency types between transactions
    // Requirement 4.2, 4.3, 4.4: Identify WAR, RAW, WAW dependency types
    enum class DependencyType {
        NONE,           // No dependency exists
        WAR,            // Write-After-Read: tx1 reads A, tx2 writes A
        RAW,            // Read-After-Write: tx1 writes A, tx2 reads A
        WAW,            // Write-After-Write: tx1 writes A, tx2 writes A
        MULTIPLE        // Multiple dependency types exist
    };
    
    // Check if two transactions have any dependency relationship
    // Requirement 4.1: Determine if two transactions are dependent
    // Returns: true if any dependency exists (WAR, RAW, or WAW), false otherwise
    static bool hasDependency(
        const std::shared_ptr<TransactionNode>& tx1,
        const std::shared_ptr<TransactionNode>& tx2);
    
    // Check if read-write sets have any intersection
    // Requirement 4.5: Efficient set intersection detection
    // Returns: true if any intersection exists between the RW sets
    static bool hasRWSetIntersection(
        const std::set<std::string>& read_keys1,
        const std::set<std::string>& write_keys1,
        const std::set<std::string>& read_keys2,
        const std::set<std::string>& write_keys2);
    
    // Get the specific dependency type between two transactions
    // Requirement 4.2, 4.3, 4.4: Identify WAR, RAW, WAW dependency types
    // Returns: The type of dependency (NONE, WAR, RAW, WAW, or MULTIPLE)
    static DependencyType getDependencyType(
        const std::shared_ptr<TransactionNode>& tx1,
        const std::shared_ptr<TransactionNode>& tx2);
    
    // Convert dependency type to string for debugging
    static std::string dependencyTypeToString(DependencyType type);

private:
    // Check if two sets have any intersection
    // Requirement 4.5: Efficient O(m+n) set intersection algorithm
    // Uses std::set_intersection for optimal performance
    template<typename T>
    static bool hasIntersection(const std::set<T>& set1, const std::set<T>& set2);
    
    // Check specific dependency types
    static bool hasWARDependency(
        const std::set<std::string>& read_keys1,
        const std::set<std::string>& write_keys2);
    
    static bool hasRAWDependency(
        const std::set<std::string>& write_keys1,
        const std::set<std::string>& read_keys2);
    
    static bool hasWAWDependency(
        const std::set<std::string>& write_keys1,
        const std::set<std::string>& write_keys2);
};

// Template implementation for hasIntersection
// Must be in header for template instantiation
template<typename T>
bool DependencyDetector::hasIntersection(const std::set<T>& set1, const std::set<T>& set2) {
    // Early exit for empty sets
    if (set1.empty() || set2.empty()) {
        return false;
    }
    
    // Use std::set_intersection with a custom output iterator that stops at first match
    // This is more efficient than computing the full intersection
    auto it1 = set1.begin();
    auto it2 = set2.begin();
    
    while (it1 != set1.end() && it2 != set2.end()) {
        if (*it1 < *it2) {
            ++it1;
        } else if (*it2 < *it1) {
            ++it2;
        } else {
            // Found an intersection
            return true;
        }
    }
    
    return false;
}

} // namespace scheduling

#endif //NEUBLOCKCHAIN_DEPENDENCY_DETECTOR_H
