#include "segment.h"
#include "notifier.h"

using MsgListener = std::function<void(const char* data, size_t)>;

class ShmReceiver{
public:
    bool Init(const std::string& channel_name, uint64_t channel_id, const std::string& endpoint, MsgListener callback);
    void OnReadable(const MessageInfo& info);
private:
    Segment segment_;
    UdsNotifier notifier_;
    MsgListener callback_;
    uint64_t channel_id_;
};

bool ShmReceiver::Init(const std::string& channel_name, uint64_t channel_id, const std::string& endpoint, MsgListener callback){
    segment_.Init(channel_name);
    callback_ = callback;
    channel_id_ = channel_id;
    if (!notifier_.Init()) {
        return false;
    }
    return notifier_.Listen(endpoint, [this](const MessageInfo& info){
        OnReadable(info);
    });
}

void ShmReceiver::OnReadable(const MessageInfo& info){
    if(info.channel_id != channel_id_){
        return;
    }
    if(segment_.AcquireBlockToRead(info.block_idx)){
        char *data = segment_.GetStartAddrOfBlock(info.block_idx);
        std::vector<char> buf(info.size);
        memcpy(buf.data(), data, info.size);
        segment_.ReleaseReadLock(info.block_idx);
        callback_(buf.data(), info.size);
    }
}