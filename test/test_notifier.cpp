#include <cassert>
#include <gtest/gtest.h>
#include "../src/notifier.h"

//g++ -std=c++17 test_notifier.cpp /usr/lib/libgtest.a /usr/lib/libgtest_main.a -lrt -pthread -o test_notifie

std::ostream& operator<<(std::ostream& os, const MessageInfo& info){
    os << "channel_id: " << info.channel_id << ", block_idx: " << info.block_idx
       << ", seq_num: " << info.seq_num << ", sender_id: " << info.sender_id;
    return os;
}

TEST(UdsNotifierTest, ListenAndNotify){
    UdsNotifier notifier;
    ASSERT_TRUE(notifier.Init());

    std::string channel_name = "/test";
    std::hash<std::string> id_hash;
    uint64_t channel_id = id_hash(channel_name);
    std::string sender_name("\0/sender", 8);
    std::string reader_name("\0/reader", 8);
    uint64_t sender_id = id_hash(sender_name);
    std::function<void(const MessageInfo&)> callback = [](const MessageInfo& info){
        std::cout<<info<<std::endl;     
    };

    ASSERT_TRUE(notifier.Listen(reader_name, callback));

    ASSERT_TRUE(notifier.Notify(MessageInfo{channel_id, 0,0, sender_id}, reader_name));

    sleep(2);
    
}