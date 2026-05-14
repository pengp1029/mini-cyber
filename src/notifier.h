/*
    事件驱动notifier
    功能就是通过unix domain socket建立worker之间的通信
*/
#include <atomic>
#include <string>
#include <functional>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>



struct MessageInfo{
    uint64_t channel_id;
    uint32_t block_idx;
    uint64_t seq_num;
    uint64_t sender_id;
};

class UdsNotifier{
public:
    bool Init();
    bool Notify(const MessageInfo& info, const std::string& reader_endpoint);
    bool Listen(const std::string& endpoint, std::function<void(const MessageInfo&)> callback);
    ~UdsNotifier(){
        running_ = 0;
        if (worker_thread_.joinable()){
            worker_thread_.join();
        }
        close(send_fd_);
        close(listen_fd_);
    }
private:
    int send_fd_;
    int listen_fd_;
    std::thread worker_thread_;
    std::atomic<int> running_{1};
};

bool UdsNotifier::Init(){
    send_fd_ = socket(AF_UNIX, SOCK_DGRAM, 0);
    listen_fd_ = socket(AF_UNIX, SOCK_DGRAM, 0);
    if(send_fd_ < 0 || listen_fd_ < 0){
        return false;
    }
    return true;
}

bool UdsNotifier::Listen(const std::string& endpoint, std::function<void(const MessageInfo&)> callback){
    // socket绑定endpoint
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, endpoint.data(), endpoint.size());
    socklen_t addr_len = offsetof(sockaddr_un, sun_path) + endpoint.size();
    if(bind(listen_fd_, (struct sockaddr*)&addr, addr_len) < 0){
        return false;
    }
    
    // 创建epoll循环
    int epoll_fd = epoll_create1(0);
    struct epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd_;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd_, &ev);
    worker_thread_ = std::thread([this, epoll_fd, callback](){
        struct epoll_event events[1];
        while(running_){
            int n = epoll_wait(epoll_fd, events, 1, 100);
            if(n>0){
                MessageInfo info{};
                recvfrom(listen_fd_, &info, sizeof(info), 0, nullptr, nullptr);
                callback(info);
            }
        }
        close(epoll_fd);
    });
    return true;
}

bool UdsNotifier::Notify(const MessageInfo& info, const std::string& reader_endpoint){
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, reader_endpoint.data(), reader_endpoint.size());
    socklen_t addr_len = offsetof(sockaddr_un, sun_path) + reader_endpoint.size();
    // 发送消息
    sendto(send_fd_, &info, sizeof(info), 0, (struct sockaddr*)&addr, addr_len);
    return true;
}
