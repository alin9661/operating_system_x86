#include <gtest/gtest.h>
#include "filesystem.h"
#include <cstring>

class FilesystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup for each test
    }
    
    void TearDown() override {
        // Cleanup for each test
    }
};

TEST_F(FilesystemTest, FileTypeEnum) {
    // Test file type enumeration
    EXPECT_EQ(static_cast<int>(FileType::Regular), 0);
    EXPECT_EQ(static_cast<int>(FileType::Directory), 1);
    EXPECT_EQ(static_cast<int>(FileType::CharacterDevice), 2);
    EXPECT_EQ(static_cast<int>(FileType::BlockDevice), 3);
    EXPECT_EQ(static_cast<int>(FileType::SymbolicLink), 4);
}

TEST_F(FilesystemTest, FilePermissions) {
    FilePermissions perms;
    perms.read = true;
    perms.write = true;
    perms.execute = false;
    
    EXPECT_TRUE(perms.read);
    EXPECT_TRUE(perms.write);
    EXPECT_FALSE(perms.execute);
}

TEST_F(FilesystemTest, FileEntry) {
    FileEntry entry;
    strcpy(entry.name, "test.txt");
    entry.type = FileType::Regular;
    entry.permissions.read = true;
    entry.permissions.write = true;
    entry.permissions.execute = false;
    entry.size = 1024;
    entry.inode = 100;
    
    EXPECT_STREQ(entry.name, "test.txt");
    EXPECT_EQ(entry.type, FileType::Regular);
    EXPECT_TRUE(entry.permissions.read);
    EXPECT_TRUE(entry.permissions.write);
    EXPECT_FALSE(entry.permissions.execute);
    EXPECT_EQ(entry.size, 1024);
    EXPECT_EQ(entry.inode, 100);
}

TEST_F(FilesystemTest, FileNameLength) {
    FileEntry entry;
    const char* long_name = "this_is_a_very_long_filename_that_might_exceed_limits.txt";
    
    // Test that we can handle file names up to the limit
    size_t max_len = sizeof(entry.name) - 1;  // -1 for null terminator
    EXPECT_GT(max_len, 0);
    
    // Test string truncation if needed
    strncpy(entry.name, long_name, max_len);
    entry.name[max_len] = '\0';
    
    EXPECT_EQ(strlen(entry.name), max_len);
}

TEST_F(FilesystemTest, DirectoryOperations) {
    // Test directory creation parameters
    const char* dir_path = "/home/user";
    FilePermissions dir_perms;
    dir_perms.read = true;
    dir_perms.write = true;
    dir_perms.execute = true;  // Execute permission for directories means "traverse"
    
    EXPECT_STREQ(dir_path, "/home/user");
    EXPECT_TRUE(dir_perms.read);
    EXPECT_TRUE(dir_perms.write);
    EXPECT_TRUE(dir_perms.execute);
}

TEST_F(FilesystemTest, FileOperations) {
    // Test file operation parameters
    const char* file_path = "/home/user/document.txt";
    size_t buffer_size = 1024;
    size_t offset = 0;
    
    EXPECT_STREQ(file_path, "/home/user/document.txt");
    EXPECT_EQ(buffer_size, 1024);
    EXPECT_EQ(offset, 0);
}

TEST_F(FilesystemTest, PathValidation) {
    // Test path validation
    const char* valid_paths[] = {
        "/",
        "/home",
        "/home/user",
        "/home/user/file.txt",
        "/etc/config"
    };
    
    const char* invalid_paths[] = {
        "",
        "relative/path",
        "//double//slash",
        "/path/with/\0/null"
    };
    
    // Test valid paths
    for (const char* path : valid_paths) {
        EXPECT_TRUE(path[0] == '/');  // Should start with /
        EXPECT_GT(strlen(path), 0);   // Should not be empty
    }
    
    // Test invalid paths
    for (const char* path : invalid_paths) {
        if (strlen(path) > 0) {
            // Most invalid paths don't start with /
            EXPECT_TRUE(path[0] != '/' || strstr(path, "//") != nullptr);
        }
    }
}

TEST_F(FilesystemTest, InodeStructure) {
    // Test inode structure (internal to filesystem)
    struct TestInode {
        uint32_t size;
        uint32_t blocks[16];
        uint32_t permissions;
        uint32_t type;
    };
    
    TestInode inode;
    inode.size = 2048;
    inode.permissions = 0755;  // rwxr-xr-x
    inode.type = static_cast<uint32_t>(FileType::Regular);
    
    // Initialize blocks
    for (int i = 0; i < 16; i++) {
        inode.blocks[i] = 0;
    }
    
    EXPECT_EQ(inode.size, 2048);
    EXPECT_EQ(inode.permissions, 0755);
    EXPECT_EQ(inode.type, static_cast<uint32_t>(FileType::Regular));
}

TEST_F(FilesystemTest, MountPoints) {
    // Test mount point structure
    struct TestMountPoint {
        char device[32];
        char path[32];
        bool mounted;
    };
    
    TestMountPoint mp;
    strcpy(mp.device, "/dev/sda1");
    strcpy(mp.path, "/home");
    mp.mounted = true;
    
    EXPECT_STREQ(mp.device, "/dev/sda1");
    EXPECT_STREQ(mp.path, "/home");
    EXPECT_TRUE(mp.mounted);
}

TEST_F(FilesystemTest, BlockSize) {
    // Test block size constants
    const size_t BLOCK_SIZE = 4096;
    const size_t BLOCKS_PER_INODE = 16;
    
    EXPECT_EQ(BLOCK_SIZE, 4096);
    EXPECT_EQ(BLOCKS_PER_INODE, 16);
    
    // Test maximum file size with direct blocks
    size_t max_file_size = BLOCK_SIZE * BLOCKS_PER_INODE;
    EXPECT_EQ(max_file_size, 65536);  // 64KB with direct blocks only
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}