/*
    Block 管理单个共享内存块
*/
#include<atomic>

class Block{
public:
    bool TryLockForWrite();
    bool TryLockForRead();
    bool ReleaseWriteLock();
    bool ReleaseReadLock();
private:
    std::atomic<bool> is_writing_{false};
    std::atomic<int> reading_ref_{0};
};

bool Block::TryLockForWrite(){
    if(!is_writing_.exchange(true, std::memory_order_acquire)){
        if (reading_ref_.load(std::memory_order_relaxed) > 0){
            is_writing_.store(false, std::memory_order_release);
            return false;
        }
        return true;
    }
    return false;
}   

bool Block::TryLockForRead(){
    if(is_writing_.load(std::memory_order_relaxed)){
        return false;
    }
    reading_ref_.fetch_add(1, std::memory_order_acquire);
    if(is_writing_.load(std::memory_order_relaxed)){
        reading_ref_.fetch_sub(1, std::memory_order_relaxed);
    }else{
        return true;
    }
    return false;
}

bool Block::ReleaseReadLock(){
    reading_ref_.fetch_sub(1, std::memory_order_release);
    return true;
}

bool Block::ReleaseWriteLock(){
    is_writing_.store(false, std::memory_order_release);
    return true;
}