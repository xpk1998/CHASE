#ifndef NEUBLOCKCHAIN_CYCLIC_BARRIER_H
#define NEUBLOCKCHAIN_CYCLIC_BARRIER_H

#include <mutex>
#include <condition_variable>

class CyclicBarrier {
public:
    explicit CyclicBarrier(unsigned int count) : threshold_(count), count_(count), generation_(0) {}

    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        unsigned int gen = generation_;
        if (--count_ == 0) {
            generation_++;
            count_ = threshold_;
            condition_.notify_all();
        } else {
            condition_.wait(lock, [this, gen] { return gen != generation_; });
        }
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    unsigned int threshold_;
    unsigned int count_;
    unsigned int generation_;
};

#endif // NEUBLOCKCHAIN_CYCLIC_BARRIER_H