#include <gtest/gtest.h>
#include "../src/shmreceiver.h"
#include "../src/shmtransmitter.h"
#include <future>

TEST(ShmTransportTest, TransmitAndReceiver){
    std::string channel_name = "/test_channel";
    std::string reader_endpoint("\0/test_reader", 13);

    std::promise<std::string> received_data_promise;
    std::function<void(const char* data, size_t size)> callback = [&received_data_promise](const char* data, size_t size){
        received_data_promise.set_value(std::string(data, size));
    };
    ShmReceiver receiver;
    std::hash<std::string> id_hash;
    uint64_t channel_id = id_hash(channel_name);
    ASSERT_TRUE(receiver.Init(channel_name, channel_id, reader_endpoint, callback));
    
    ShmTransmitter transmitter;
    ASSERT_TRUE(transmitter.Init(channel_name, channel_id));
    transmitter.AddReceiver(reader_endpoint);
    std::string data = "Hello, World";
    ASSERT_TRUE(transmitter.Transmit(data.data(), data.size(), 0));
    auto received_data = received_data_promise.get_future();
    ASSERT_EQ(received_data.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    EXPECT_TRUE(received_data.get() == data);
}