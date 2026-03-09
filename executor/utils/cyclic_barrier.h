#ifndef NEU_BLOCKCHAIN_CYCLIC_BARRIER_H
#define NEU_BLOCKCHAIN_CYCLIC_BARRIER_H

#include <mutex>
#include <condition_variable>
#include <functional>

namespace cbar {

class CyclicBarrier {
public:
    explicit CyclicBarrier(uint32_t parties, std::function<void()> callback = nullptr);
    
    void await(uint64_t nano_secs = 0);
    void reset();
    
    [[nodiscard]] uint32_t get_barrier_size() const;
    [[nodiscard]] uint32_t get_current_waiting() const;

private:
    mutable std::mutex mutex;
    std::condition_variable cv;
    const uint32_t parties;
    uint32_t current_waits;
    std::function<void()> callback;
};

} // namespace cbar

#endif //NEU_BLOCKCHAIN_CYCLIC_BARRIER_H