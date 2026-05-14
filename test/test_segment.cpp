#include <gtest/gtest.h>
#include <sys/mman.h>
#include "../src/segment.h"

//g++ -std=c++17 test_segment.cpp     /usr/lib/libgtest.a     /usr/lib/libgtest_main.a     -lrt -pthread -o test_segment
TEST(SegmentTest, BasicWriteRead){
    Segment seg;
    seg.Init("/test");

    int idx = seg.AcquireBlockToWrite();
    ASSERT_GE(idx, 0);

    char *buf = seg.GetStartAddrOfBlock(idx);
    memcpy(buf, "hello", 5);
    seg.ReleaseWrittenBlock(idx);

    ASSERT_TRUE(seg.AcquireBlockToRead(idx));
    EXPECT_EQ(memcmp(buf, "hello", 5), 0);
    seg.ReleaseReadLock(idx);
    shm_unlink("/test");
}

TEST(SegmentTest, WriteBlocksRead){
    Segment seg;
    seg.Init("/test");
    
    int idx = seg.AcquireBlockToWrite();
    EXPECT_FALSE(seg.AcquireBlockToRead(idx));
    seg.ReleaseWrittenBlock(idx);
    shm_unlink("/test");
}

int main(int args, char** argv){
    testing::InitGoogleTest(&args, argv);
    return RUN_ALL_TESTS();
}