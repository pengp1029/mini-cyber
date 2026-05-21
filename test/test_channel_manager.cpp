#include <gtest/gtest.h>
#include "../src/channel_manager.h"

TEST(ChannelManagerTest, JoinAndLeave){
    ChannelManager& manager = ChannelManager::GetInstance();
    manager.Init();

    std::string channel_name = "/test_channel";
    std::string wendpoint = "/test_wendpoint";
    std::string rendpoint = "/test_rendpoint";
    std::string shm_name = "/test_shm";
    manager.Join(channel_name, wendpoint, shm_name, 1); // Writer
    
    ASSERT_EQ(manager.GetReaderEndpoints(channel_name).size(), 0); // Should be empty

    manager.Join(channel_name, rendpoint, shm_name, 2); // Reader

    std::vector<std::string> reader_endpoints = manager.GetReaderEndpoints(channel_name); // Should be empty
    ASSERT_EQ(reader_endpoints.size(), 1);
    ASSERT_TRUE(reader_endpoints[0] == rendpoint);
    manager.Leave(channel_name, 1);
}
