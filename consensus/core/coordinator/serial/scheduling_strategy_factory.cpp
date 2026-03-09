#include "scheduling_strategy.h"
#include "../optme/optme_scheduling_strategy.h"
#include "../block-stm/blockstm_scheduling_strategy.h"
#include "../lcdps/lcdps_scheduling_strategy.h"
#include "serial_scheduling_strategy.h"
#include <glog/logging.h>
#include <algorithm>
#include <cctype>

namespace scheduling {

std::unique_ptr<SchedulingStrategy> SchedulingStrategyFactory::create(const std::string& name) {
    // Convert name to lowercase for case-insensitive comparison
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), 
                   [](unsigned char c){ return std::tolower(c); });
    
    // Create strategy based on name
    if (lower_name == "serial") {
        LOG(INFO) << "Creating Serial Scheduling Strategy";
        return std::make_unique<consensus::coordinator::serial::SerialSchedulingStrategy>();
    }
    else if (lower_name == "optme") {
        LOG(INFO) << "Creating OptME Scheduling Strategy";
        return std::make_unique<consensus::coordinator::optme::OptmeSchedulingStrategy>();
    }
    else if (lower_name == "blockstm" || lower_name == "block-stm") {
        LOG(INFO) << "Creating Block-STM Scheduling Strategy";
        return std::make_unique<consensus::coordinator::blockstm::BlockStmSchedulingStrategy>();
    }
    else if (lower_name == "lcdps" || lower_name == "neuchain" || lower_name == "blp") {
        LOG(INFO) << "Creating LCDPS Scheduling Strategy";
        return std::make_unique<consensus::coordinator::lcdps::LcdpsSchedulingStrategy>();
    }
    else {
        LOG(ERROR) << "Unknown scheduling strategy: " << name;
        return nullptr;
    }
}

bool SchedulingStrategyFactory::isValidStrategyName(const std::string& name) {
    // Convert name to lowercase for case-insensitive comparison
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), 
                   [](unsigned char c){ return std::tolower(c); });
    
    return (lower_name == "serial" || 
            lower_name == "optme" || 
            lower_name == "blockstm" || 
            lower_name == "block-stm" ||
            lower_name == "lcdps" || 
            lower_name == "neuchain" || 
            lower_name == "blp");
}

std::vector<std::string> SchedulingStrategyFactory::getAvailableStrategies() {
    return {"serial", "optme", "blockstm", "lcdps"};
}

}  // namespace scheduling