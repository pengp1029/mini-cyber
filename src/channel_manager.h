#include <string>
#include <vector>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <cstring>
#include <stdexcept>

#define MAX_CHANNELS 256
class ChannelManager{
public:
    static ChannelManager& GetInstance();
    void Init();
    bool Join(const std::string& channel_name,
              const std::string& endpoint,
              const std::string& shm_name,
              int role); // 1=Writer 2=Reader

    void Leave(const std::string& channel_name, int role);

    std::vector<std::string> GetReaderEndpoints(const std::string& channel_name);

    std::string GetWriterShmName(const std::string& channel_name);

private:
    
    struct RegistryEntry{
        char channel_name[64];
        char endpoint[64];
        char shm_name[64];
        int role;
        pid_t pid;
        int valid;
    };

    struct SharedRegistry{
        int magic;
        pthread_mutex_t mutex;
        RegistryEntry registry[MAX_CHANNELS];
    };

    SharedRegistry* registry_;
    void *shared_addr_;
    int fd_;
};

ChannelManager& ChannelManager::GetInstance(){
    static ChannelManager instance;
    return instance;
}

void ChannelManager::Init(){
    fd_ = shm_open("/channel_registry", O_CREAT | O_RDWR, 0666);
    if (fd_ < 0){
        perror("shm_open");
        exit(1);
    }
    int total_size = sizeof(SharedRegistry);
    ftruncate(fd_, total_size);

    shared_addr_ = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);

    registry_ = (SharedRegistry *)shared_addr_;
    if(registry_->magic != 0x12345678){
        memset(registry_, 0, sizeof(SharedRegistry));
        registry_->magic = 0x12345678;
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
        pthread_mutex_init(&registry_->mutex, &attr);
        pthread_mutexattr_destroy(&attr);

        for(int i = 0; i < MAX_CHANNELS; i++){
            registry_->registry[i].valid = 0;
        }
    }
}

bool ChannelManager::Join(const std::string& channel_name,
              const std::string& endpoint,
              const std::string& shm_name,
              int role){
    int ret = pthread_mutex_lock(&registry_->mutex);
    if (ret == EOWNERDEAD) {
        // 持锁进程崩溃了，数据可能不一致，这里简单恢复锁
        pthread_mutex_consistent(&registry_->mutex);
    }

    for(int i = 0; i < MAX_CHANNELS; i++){
        if(registry_->registry[i].valid == 0){
            strncpy(registry_->registry[i].channel_name, channel_name.data(), 63);
            strncpy(registry_->registry[i].endpoint, endpoint.data(), 63);
            strncpy(registry_->registry[i].shm_name, shm_name.data(), 63);
            registry_->registry[i].role = role;
            registry_->registry[i].pid = getpid();
            registry_->registry[i].valid = 1;
            pthread_mutex_unlock(&registry_->mutex);
            return true;
        }
    }
    pthread_mutex_unlock(&registry_->mutex);
    return false;
}

std::vector<std::string> ChannelManager::GetReaderEndpoints(const std::string& channel_name){
    std::vector<std::string> endpoints;
    int ret = pthread_mutex_lock(&registry_->mutex);
    if (ret == EOWNERDEAD) {
        // 持锁进程崩溃了，数据可能不一致，这里简单恢复锁
        pthread_mutex_consistent(&registry_->mutex);
    }
    for(int i = 0; i < MAX_CHANNELS; i++){
        if(registry_->registry[i].valid == 1 &&
           registry_->registry[i].role == 2 &&
           strncmp(registry_->registry[i].channel_name, channel_name.data(), 63) == 0){
            endpoints.push_back(std::string(registry_->registry[i].endpoint));
        }
    }
    pthread_mutex_unlock(&registry_->mutex);
    return endpoints;
}

std::string ChannelManager::GetWriterShmName(const std::string& channel_name){
    int ret = pthread_mutex_lock(&registry_->mutex);
    if (ret == EOWNERDEAD) {
        // 持锁进程崩溃了，数据可能不一致，这里简单恢复锁
        pthread_mutex_consistent(&registry_->mutex);
    }
    for(int i = 0; i<MAX_CHANNELS; i++){
        if(registry_->registry[i].valid == 1 && registry_->registry[i].role == 1 && strncmp(registry_->registry[i].channel_name, channel_name.data(), 63)==0){
            std::string shm_name(registry_->registry[i].shm_name);
            pthread_mutex_unlock(&registry_->mutex);
            return shm_name;
        }
    }
    pthread_mutex_unlock(&registry_->mutex);
    throw std::runtime_error("Writer shared memory name not found");
}

void ChannelManager::Leave(const std::string& channel_name, int role){
    int ret = pthread_mutex_lock(&registry_->mutex);
    if (ret == EOWNERDEAD) {
        // 持锁进程崩溃了，数据可能不一致，这里简单恢复锁
        pthread_mutex_consistent(&registry_->mutex);
    }
    for(int i = 0; i < MAX_CHANNELS; i++){
        if(registry_->registry[i].valid == 1 &&
           registry_->registry[i].role == role &&
           strncmp(registry_->registry[i].channel_name, channel_name.data(), 63) == 0 &&
           registry_->registry[i].pid == getpid()){
            registry_->registry[i].valid = 0;
        }
    }
    pthread_mutex_unlock(&registry_->mutex);
}