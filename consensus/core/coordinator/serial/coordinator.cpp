#include "coordinator.h"

namespace consensus {

Coordinator::Coordinator() 
    : current_epoch_(0), current_round_(0) {
}

Coordinator::~Coordinator() = default;

} // namespace consensus