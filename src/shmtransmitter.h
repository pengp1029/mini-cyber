
#include "segment.h"
#include "notifier.h"

class ShmTransmitter{
public:
    bool Init(const std::string& channel_name, uint64_t channel_id);
    void AddReceiver(const std::string& endpoint);
    bool Transmit(const char* data, size_t size, uint64_t seq);
private:
    Segment segment_;
    std::vector<std::string>  peer_endpoints_;
    UdsNotifier notifier_;
    uint64_t channel_id_;
    uint64_t sender_id_;
};

bool ShmTransmitter::Transmit(const char* data, size_t size, uint64_t seq){
    int index = segment_.AcquireBlockToWrite();
    if(index < 0){
        return false;
    }   
    char *block_addr = segment_.GetStartAddrOfBlock(index);
    memcpy(block_addr, data, size);
    segment_.ReleaseWrittenBlock(index);
    MessageInfo info{.channel_id = channel_id_, .block_idx = uint32_t(index), .seq_num = seq, .sender_id = sender_id_, .size = size};
    for(const auto &endpoint : peer_endpoints_){
        notifier_.Notify(info, endpoint);
    }
    return true;

}

bool ShmTransmitter::Init(const std::string& channel_name, uint64_t channel_id){
    channel_id_ = channel_id;
    sender_id_ = uint64_t(getpid() + time(NULL));
    segment_.Init(channel_name);
    return notifier_.Init();
}

void ShmTransmitter::AddReceiver(const std::string& endpoint){
    peer_endpoints_.push_back(endpoint);
}