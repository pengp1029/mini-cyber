/*
    共享内存池
*/
#include "block.h"
#include <atomic>
#include <cstring>
#include <fcntl.h>
#include <set>
#include <vector>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

#define BLOCK_DEFAULT_SIZE size_t(512 * 1024)
#define SEGMENT_DEFAULT_CAPSITY 16
class Segment{
public:
    void Init(std::string channel_name);
    int AcquireBlockToWrite();
    void ReleaseWrittenBlock(int index);
    bool AcquireBlockToRead(int index);
    void ReleaseReadLock(int index);
    char *GetStartAddrOfBlock(int index);
    ~Segment(){
        for(auto block : block_manager_){
            block -> ~Block();
        }
        // 接触映射并关闭文件描述符
        munmap(shared_addr_, total_size_);
        close(fd_);
    }
private:
    struct State{
        int magic = 0x12345678;
        std::atomic<int> ref_count{0};
    };
    std::vector<Block*> block_manager_;
    int next_slot_{0};
    int fd_;
    size_t total_size_{sizeof(State) + sizeof(Block) * SEGMENT_DEFAULT_CAPSITY + BLOCK_DEFAULT_SIZE * SEGMENT_DEFAULT_CAPSITY};
    void *shared_addr_;
    std::string channel_name_;
    State *state_;
};

void Segment::Init(std::string channel_name){
    channel_name_ = channel_name;
    fd_ = shm_open(channel_name.c_str(), O_CREAT | O_RDWR, 0666);
    if(fd_ < 0){
        perror("shm_open");
        exit(1);
    }

    size_t offset = sizeof(State) + sizeof(Block) * SEGMENT_DEFAULT_CAPSITY;
    ftruncate(fd_, total_size_);

    shared_addr_ = mmap(NULL, total_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);

    // 验证是否已经初始化
    state_ = (State *)shared_addr_;
    if(state_ -> magic == 0x12345678){
        state_ -> ref_count.fetch_add(1, std::memory_order_relaxed);
        for(int i = 0; i< SEGMENT_DEFAULT_CAPSITY; i++){
            block_manager_.push_back((Block*)((char*)shared_addr_ + sizeof(State)+ sizeof(Block)*i));
        }
        return;
    }
    memset(shared_addr_, 0, total_size_);
    state_ -> ref_count = 1;
    state_ -> magic = 0x12345678;

    // 分配16个Block空间
    Block* blocks = (Block*)((char*)shared_addr_ + sizeof(State));
    for(int i = 0; i< SEGMENT_DEFAULT_CAPSITY; i++){
        // placement new: 在指定内存地址创建对象
        block_manager_.push_back(new (&blocks[i]) Block());
    }
}

int Segment::AcquireBlockToWrite(){
    int last_slot = next_slot_;
    while(!block_manager_[next_slot_]->TryLockForWrite()){
        next_slot_ = (next_slot_ + 1) % SEGMENT_DEFAULT_CAPSITY;
        if(next_slot_ == last_slot){
            return -1;
        }
    }
    return next_slot_;
}

bool Segment::AcquireBlockToRead(int index){
    return block_manager_[index]->TryLockForRead();
}

void Segment::ReleaseWrittenBlock(int index){
    block_manager_[index]->ReleaseWriteLock();
}

void Segment::ReleaseReadLock(int index){
    block_manager_[index]->ReleaseReadLock();
}

char *Segment::GetStartAddrOfBlock(int index){
    return (char*)shared_addr_ + sizeof(State) + sizeof(Block) * SEGMENT_DEFAULT_CAPSITY + BLOCK_DEFAULT_SIZE * index;
}




