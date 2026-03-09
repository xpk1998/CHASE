#include "node/core/blockchain_node.h"
#include "utilities/config/yaml_config.h"
#include "glog/logging.h"
#include <iostream>
#include <memory>

int main() {
    // Initialize logging
    google::InitGoogleLogging("parachain_node");
    FLAGS_logtostderr = 1;
    
    std::cout << "ParaChain Node starting..." << std::endl;
    
    try {
        // Create and initialize blockchain node
        auto node = std::make_unique<parachain::BlockchainNode>();
        
        // Initialize all components
        node->initialize();
        
        // Start the node
        node->start();
        
        std::cout << "ParaChain Node started successfully!" << std::endl;
        
        // Keep the node running
        std::cout << "Press Ctrl+C to stop the node" << std::endl;
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            node->printStats();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error starting ParaChain Node: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}