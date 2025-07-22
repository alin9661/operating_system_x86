#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <expected>
#include <concepts>
#include <string>
#include <string_view>
#include <span>
#include <array>
#include <vector>
#include <unordered_map>
#include <optional>
#include <variant>
#include <chrono>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <bitset>
#include <algorithm>
#include <ranges>
#include <filesystem>
#include <fstream>

namespace FileSystem {
    // Modern filesystem constants
    inline constexpr std::size_t MAX_FILENAME_LENGTH = 255;
    inline constexpr std::size_t MAX_PATH_LENGTH = 4096;
    inline constexpr std::size_t BLOCK_SIZE = 4096;
    inline constexpr std::size_t INODE_TABLE_SIZE = 65536;
    inline constexpr std::size_t MAX_OPEN_FILES = 1024;
    inline constexpr std::size_t CACHE_SIZE = 64 * 1024 * 1024; // 64MB
    
    // Filesystem error types
    enum class FileSystemError : std::uint8_t {
        Success = 0,
        FileNotFound,
        DirectoryNotFound,
        PermissionDenied,
        FileExists,
        DirectoryExists,
        InvalidPath,
        InvalidFileName,
        DiskFull,
        ReadOnlyFileSystem,
        TooManyOpenFiles,
        IoError,
        CorruptedFileSystem,
        UnsupportedOperation,
        InsufficientMemory,
        NetworkError,
        TimeoutExpired,
        QuotaExceeded,
        SymlinkLoop,
        NameTooLong,
        NotEmpty,
        CrossDeviceLink,
        InvalidArgument
    };
    
    // File types with modern enum features
    enum class FileType : std::uint8_t {
        Unknown = 0,
        Regular = 1,        // Regular file
        Directory = 2,      // Directory
        Symlink = 3,        // Symbolic link
        Hardlink = 4,       // Hard link
        CharDevice = 5,     // Character device
        BlockDevice = 6,    // Block device
        FIFO = 7,          // Named pipe (FIFO)
        Socket = 8,         // Unix domain socket
        MountPoint = 9,     // Mount point
        Special = 10        // Special file
    };
    
    // File permissions with POSIX-style permissions
    enum class FilePermissions : std::uint16_t {
        None = 0,
        
        // Owner permissions
        OwnerRead = 0400,
        OwnerWrite = 0200,
        OwnerExecute = 0100,
        OwnerAll = OwnerRead | OwnerWrite | OwnerExecute,
        
        // Group permissions
        GroupRead = 0040,
        GroupWrite = 0020,
        GroupExecute = 0010,
        GroupAll = GroupRead | GroupWrite | GroupExecute,
        
        // Other permissions
        OtherRead = 0004,
        OtherWrite = 0002,
        OtherExecute = 0001,
        OtherAll = OtherRead | OtherWrite | OtherExecute,
        
        // Common permission combinations
        ReadOnly = OwnerRead | GroupRead | OtherRead,
        ReadWrite = OwnerRead | OwnerWrite | GroupRead | OtherRead,
        Executable = OwnerAll | GroupRead | GroupExecute | OtherRead | OtherExecute,
        Default = OwnerRead | OwnerWrite | GroupRead | OtherRead,
        
        // Special permissions
        SetUID = 04000,     // Set user ID on execution
        SetGID = 02000,     // Set group ID on execution
        StickyBit = 01000,  // Sticky bit
        
        All = 0777
    };
    
    // Enable bitwise operations on FilePermissions
    constexpr FilePermissions operator|(FilePermissions a, FilePermissions b) noexcept {
        return static_cast<FilePermissions>(static_cast<std::uint16_t>(a) | static_cast<std::uint16_t>(b));
    }
    
    constexpr FilePermissions operator&(FilePermissions a, FilePermissions b) noexcept {
        return static_cast<FilePermissions>(static_cast<std::uint16_t>(a) & static_cast<std::uint16_t>(b));
    }
    
    constexpr FilePermissions operator~(FilePermissions a) noexcept {
        return static_cast<FilePermissions>(~static_cast<std::uint16_t>(a));
    }
    
    // File access modes
    enum class AccessMode : std::uint8_t {
        ReadOnly = 0,
        WriteOnly = 1,
        ReadWrite = 2,
        Append = 3,
        Create = 4,
        Truncate = 5,
        Exclusive = 6
    };
    
    // File seek origins
    enum class SeekOrigin : std::uint8_t {
        Begin = 0,      // SEEK_SET
        Current = 1,    // SEEK_CUR
        End = 2         // SEEK_END
    };
    
    // Filesystem types
    enum class FileSystemType : std::uint32_t {
        Unknown = 0,
        EXT4 = 0x53EF,      // Linux EXT4
        NTFS = 0x5346544E,  // Windows NTFS
        FAT32 = 0x46415432, // FAT32
        XFS = 0x58465300,   // XFS
        BTRFS = 0x9123683E, // Btrfs
        ZFS = 0x2FC12FC1,   // ZFS
        TMPFS = 0x01021994, // tmpfs
        PROCFS = 0x9FA0,    // procfs
        SYSFS = 0x62656572, // sysfs
        ModernFS = 0x4D6F64 // Our custom modern filesystem
    };
    
    // Inode number type
    using InodeNumber = std::uint64_t;
    using BlockNumber = std::uint64_t;
    using FileDescriptor = std::int32_t;
    using DeviceId = std::uint32_t;
    using UserId = std::uint32_t;
    using GroupId = std::uint32_t;
    
    // Special inode numbers
    inline constexpr InodeNumber INVALID_INODE = 0;
    inline constexpr InodeNumber ROOT_INODE = 1;
    inline constexpr InodeNumber JOURNAL_INODE = 2;
    inline constexpr InodeNumber BITMAP_INODE = 3;
    
    // Special file descriptors
    inline constexpr FileDescriptor INVALID_FD = -1;
    inline constexpr FileDescriptor STDIN_FD = 0;
    inline constexpr FileDescriptor STDOUT_FD = 1;
    inline constexpr FileDescriptor STDERR_FD = 2;
    
    // Forward declarations
    class FileSystem;
    class VirtualFileSystem;
    class Inode;
    class Directory;
    class File;
    class FileHandle;
    class FilesystemCache;
    class Journal;
    
    // Modern file metadata with C++20 features
    struct FileMetadata {
        InodeNumber inode;
        FileType type;
        FilePermissions permissions;
        std::uint64_t size;
        std::uint64_t blocks;
        DeviceId device;
        UserId owner_uid;
        GroupId owner_gid;
        std::uint32_t hard_links;
        
        // Timestamps
        std::chrono::system_clock::time_point access_time;
        std::chrono::system_clock::time_point modify_time;
        std::chrono::system_clock::time_point change_time;
        std::chrono::system_clock::time_point birth_time;
        
        // Extended attributes
        std::unordered_map<std::string, std::string> extended_attributes;
        
        // C++20 defaulted comparison operators
        auto operator<=>(const FileMetadata&) const = default;
        
        // Utility functions
        [[nodiscard]] bool is_regular_file() const noexcept {
            return type == FileType::Regular;
        }
        
        [[nodiscard]] bool is_directory() const noexcept {
            return type == FileType::Directory;
        }
        
        [[nodiscard]] bool is_symlink() const noexcept {
            return type == FileType::Symlink;
        }
        
        [[nodiscard]] bool is_executable() const noexcept {
            return (permissions & FilePermissions::OwnerExecute) != FilePermissions::None ||
                   (permissions & FilePermissions::GroupExecute) != FilePermissions::None ||
                   (permissions & FilePermissions::OtherExecute) != FilePermissions::None;
        }
        
        [[nodiscard]] bool can_read(UserId uid, GroupId gid) const noexcept {
            if (uid == owner_uid) {
                return (permissions & FilePermissions::OwnerRead) != FilePermissions::None;
            }
            if (gid == owner_gid) {
                return (permissions & FilePermissions::GroupRead) != FilePermissions::None;
            }
            return (permissions & FilePermissions::OtherRead) != FilePermissions::None;
        }
        
        [[nodiscard]] bool can_write(UserId uid, GroupId gid) const noexcept {
            if (uid == owner_uid) {
                return (permissions & FilePermissions::OwnerWrite) != FilePermissions::None;
            }
            if (gid == owner_gid) {
                return (permissions & FilePermissions::GroupWrite) != FilePermissions::None;
            }
            return (permissions & FilePermissions::OtherWrite) != FilePermissions::None;
        }
        
        [[nodiscard]] std::chrono::milliseconds age() const noexcept {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now() - birth_time);
        }
    };
    
    // Directory entry with modern features
    struct DirectoryEntry {
        InodeNumber inode;
        std::string name;
        FileType type;
        std::uint32_t record_length;
        
        // C++20 defaulted comparison operators
        auto operator<=>(const DirectoryEntry&) const = default;
        
        [[nodiscard]] bool is_dot_entry() const noexcept {
            return name == "." || name == "..";
        }
        
        [[nodiscard]] bool is_hidden() const noexcept {
            return !name.empty() && name[0] == '.';
        }
    };
    
    // File system statistics
    struct FileSystemStatistics {
        std::uint64_t total_blocks;
        std::uint64_t free_blocks;
        std::uint64_t available_blocks;
        std::uint64_t total_inodes;
        std::uint64_t free_inodes;
        std::uint32_t block_size;
        std::uint32_t max_filename_length;
        FileSystemType type;
        std::string mount_point;
        std::string device_name;
        
        // Cache statistics
        std::atomic<std::uint64_t> cache_hits{0};
        std::atomic<std::uint64_t> cache_misses{0};
        std::atomic<std::uint64_t> cache_writes{0};
        std::atomic<std::uint64_t> cache_reads{0};
        
        // I/O statistics
        std::atomic<std::uint64_t> read_operations{0};
        std::atomic<std::uint64_t> write_operations{0};
        std::atomic<std::uint64_t> bytes_read{0};
        std::atomic<std::uint64_t> bytes_written{0};
        
        [[nodiscard]] double utilization() const noexcept {
            return total_blocks > 0 ? 
                static_cast<double>(total_blocks - free_blocks) / total_blocks : 0.0;
        }
        
        [[nodiscard]] double cache_hit_ratio() const noexcept {
            const auto total = cache_hits.load() + cache_misses.load();
            return total > 0 ? static_cast<double>(cache_hits.load()) / total : 0.0;
        }
    };
    
    // File handle concept
    template<typename T>
    concept FileHandleType = requires(T handle) {
        { handle.read(std::span<std::byte>{}) } -> std::same_as<std::expected<std::size_t, FileSystemError>>;
        { handle.write(std::span<const std::byte>{}) } -> std::same_as<std::expected<std::size_t, FileSystemError>>;
        { handle.seek(std::int64_t{}, SeekOrigin{}) } -> std::same_as<std::expected<std::uint64_t, FileSystemError>>;
        { handle.close() } -> std::same_as<std::expected<void, FileSystemError>>;
    };
    
    // Path concept
    template<typename T>
    concept PathType = std::convertible_to<T, std::string_view> ||
                      std::same_as<T, std::filesystem::path> ||
                      std::same_as<T, const char*>;
    
    // File system result type
    template<typename T>
    using FSResult = std::expected<T, FileSystemError>;
    
    // RAII file handle
    class FileHandle {
    private:
        FileDescriptor fd_;
        std::string path_;
        AccessMode mode_;
        std::atomic<std::uint64_t> position_{0};
        std::atomic<bool> is_open_{false};
        
    public:
        explicit FileHandle(FileDescriptor fd, std::string path, AccessMode mode) noexcept
            : fd_(fd), path_(std::move(path)), mode_(mode), is_open_(true) {}
        
        ~FileHandle() noexcept {
            if (is_open_.load()) {
                close();
            }
        }
        
        // Move-only semantics
        FileHandle(const FileHandle&) = delete;
        FileHandle& operator=(const FileHandle&) = delete;
        
        FileHandle(FileHandle&& other) noexcept
            : fd_(other.fd_), path_(std::move(other.path_)), mode_(other.mode_),
              position_(other.position_.load()), is_open_(other.is_open_.load()) {
            other.fd_ = INVALID_FD;
            other.is_open_ = false;
        }
        
        FileHandle& operator=(FileHandle&& other) noexcept {
            if (this != &other) {
                if (is_open_.load()) {
                    close();
                }
                fd_ = other.fd_;
                path_ = std::move(other.path_);
                mode_ = other.mode_;
                position_.store(other.position_.load());
                is_open_.store(other.is_open_.load());
                other.fd_ = INVALID_FD;
                other.is_open_ = false;
            }
            return *this;
        }
        
        // File operations
        [[nodiscard]] FSResult<std::size_t> read(std::span<std::byte> buffer) noexcept;
        [[nodiscard]] FSResult<std::size_t> write(std::span<const std::byte> data) noexcept;
        [[nodiscard]] FSResult<std::uint64_t> seek(std::int64_t offset, SeekOrigin origin) noexcept;
        [[nodiscard]] FSResult<void> flush() noexcept;
        [[nodiscard]] FSResult<void> sync() noexcept;
        [[nodiscard]] FSResult<void> close() noexcept;
        
        // Status and properties
        [[nodiscard]] FileDescriptor descriptor() const noexcept { return fd_; }
        [[nodiscard]] const std::string& path() const noexcept { return path_; }
        [[nodiscard]] AccessMode mode() const noexcept { return mode_; }
        [[nodiscard]] std::uint64_t position() const noexcept { return position_.load(); }
        [[nodiscard]] bool is_open() const noexcept { return is_open_.load(); }
        
        // Advanced operations
        [[nodiscard]] FSResult<FileMetadata> get_metadata() const noexcept;
        [[nodiscard]] FSResult<void> set_permissions(FilePermissions permissions) noexcept;
        [[nodiscard]] FSResult<void> set_owner(UserId uid, GroupId gid) noexcept;
        [[nodiscard]] FSResult<void> truncate(std::uint64_t size) noexcept;
        [[nodiscard]] FSResult<void> lock(bool exclusive = true) noexcept;
        [[nodiscard]] FSResult<void> unlock() noexcept;
    };
    
    // Path manipulation utilities
    class Path {
    private:
        std::string path_;
        
    public:
        explicit Path(std::string_view path = "/") : path_(path) {
            normalize();
        }
        
        // Path operations
        [[nodiscard]] Path parent() const noexcept;
        [[nodiscard]] std::string filename() const noexcept;
        [[nodiscard]] std::string extension() const noexcept;
        [[nodiscard]] std::string stem() const noexcept;
        [[nodiscard]] bool is_absolute() const noexcept;
        [[nodiscard]] bool is_relative() const noexcept;
        [[nodiscard]] bool is_root() const noexcept;
        
        // Path manipulation
        Path& append(std::string_view component) noexcept;
        Path& replace_filename(std::string_view filename) noexcept;
        Path& replace_extension(std::string_view extension) noexcept;
        [[nodiscard]] Path canonical() const noexcept;
        [[nodiscard]] Path relative_to(const Path& base) const noexcept;
        
        // Operators
        Path operator/(std::string_view component) const noexcept;
        Path& operator/=(std::string_view component) noexcept;
        
        // String conversion
        [[nodiscard]] const std::string& string() const noexcept { return path_; }
        [[nodiscard]] const char* c_str() const noexcept { return path_.c_str(); }
        
        // Comparison
        auto operator<=>(const Path&) const = default;
        
    private:
        void normalize() noexcept;
    };
    
    // Modern filesystem interface with C++20 features
    
    /// Initialize the file system subsystem
    /// @return Success or error code
    [[nodiscard]] FSResult<void> init() noexcept;
    
    /// Mount a filesystem
    /// @param device Device path
    /// @param mount_point Mount point path
    /// @param fs_type Filesystem type
    /// @param options Mount options
    /// @return Success or error code
    [[nodiscard]] FSResult<void> mount(
        std::string_view device,
        const Path& mount_point,
        FileSystemType fs_type = FileSystemType::Unknown,
        std::string_view options = ""
    ) noexcept;
    
    /// Unmount a filesystem
    /// @param mount_point Mount point path
    /// @param force Force unmount
    /// @return Success or error code
    [[nodiscard]] FSResult<void> unmount(const Path& mount_point, bool force = false) noexcept;
    
    /// Open a file
    /// @param path File path
    /// @param mode Access mode
    /// @return File handle or error
    template<PathType P>
    [[nodiscard]] FSResult<FileHandle> open(P&& path, AccessMode mode = AccessMode::ReadOnly) noexcept;
    
    /// Create a file
    /// @param path File path
    /// @param permissions File permissions
    /// @return Success or error code
    template<PathType P>
    [[nodiscard]] FSResult<void> create_file(
        P&& path,
        FilePermissions permissions = FilePermissions::Default
    ) noexcept;
    
    /// Create a directory
    /// @param path Directory path
    /// @param permissions Directory permissions
    /// @param recursive Create parent directories
    /// @return Success or error code
    template<PathType P>
    [[nodiscard]] FSResult<void> create_directory(
        P&& path,
        FilePermissions permissions = FilePermissions::Default,
        bool recursive = false
    ) noexcept;
    
    /// Remove a file or directory
    /// @param path Path to remove
    /// @param recursive Remove recursively (for directories)
    /// @return Success or error code
    template<PathType P>
    [[nodiscard]] FSResult<void> remove(P&& path, bool recursive = false) noexcept;
    
    /// Copy a file or directory
    /// @param source Source path
    /// @param destination Destination path
    /// @param overwrite Overwrite if destination exists
    /// @return Success or error code
    template<PathType P1, PathType P2>
    [[nodiscard]] FSResult<void> copy(P1&& source, P2&& destination, bool overwrite = false) noexcept;
    
    /// Move/rename a file or directory
    /// @param source Source path
    /// @param destination Destination path
    /// @return Success or error code
    template<PathType P1, PathType P2>
    [[nodiscard]] FSResult<void> move(P1&& source, P2&& destination) noexcept;
    
    /// Create a symbolic link
    /// @param target Target path
    /// @param link_path Link path
    /// @return Success or error code
    template<PathType P1, PathType P2>
    [[nodiscard]] FSResult<void> create_symlink(P1&& target, P2&& link_path) noexcept;
    
    /// Create a hard link
    /// @param target Target path
    /// @param link_path Link path
    /// @return Success or error code
    template<PathType P1, PathType P2>
    [[nodiscard]] FSResult<void> create_hardlink(P1&& target, P2&& link_path) noexcept;
    
    /// Read a symbolic link
    /// @param path Symlink path
    /// @return Target path or error
    template<PathType P>
    [[nodiscard]] FSResult<std::string> read_symlink(P&& path) noexcept;
    
    /// Check if a path exists
    /// @param path Path to check
    /// @return true if exists, false otherwise
    template<PathType P>
    [[nodiscard]] bool exists(P&& path) noexcept;
    
    /// Check if a path is a regular file
    /// @param path Path to check
    /// @return true if regular file, false otherwise
    template<PathType P>
    [[nodiscard]] bool is_regular_file(P&& path) noexcept;
    
    /// Check if a path is a directory
    /// @param path Path to check
    /// @return true if directory, false otherwise
    template<PathType P>
    [[nodiscard]] bool is_directory(P&& path) noexcept;
    
    /// Check if a path is a symbolic link
    /// @param path Path to check
    /// @return true if symlink, false otherwise
    template<PathType P>
    [[nodiscard]] bool is_symlink(P&& path) noexcept;
    
    /// Get file metadata
    /// @param path File path
    /// @param follow_symlinks Follow symbolic links
    /// @return File metadata or error
    template<PathType P>
    [[nodiscard]] FSResult<FileMetadata> get_metadata(P&& path, bool follow_symlinks = true) noexcept;
    
    /// Set file permissions
    /// @param path File path
    /// @param permissions New permissions
    /// @return Success or error code
    template<PathType P>
    [[nodiscard]] FSResult<void> set_permissions(P&& path, FilePermissions permissions) noexcept;
    
    /// Change file owner
    /// @param path File path
    /// @param uid New user ID
    /// @param gid New group ID
    /// @return Success or error code
    template<PathType P>
    [[nodiscard]] FSResult<void> change_owner(P&& path, UserId uid, GroupId gid) noexcept;
    
    /// List directory contents
    /// @param path Directory path
    /// @return Vector of directory entries or error
    template<PathType P>
    [[nodiscard]] FSResult<std::vector<DirectoryEntry>> list_directory(P&& path) noexcept;
    
    /// Get filesystem statistics
    /// @param path Path on filesystem
    /// @return Filesystem statistics or error
    template<PathType P>
    [[nodiscard]] FSResult<FileSystemStatistics> get_filesystem_stats(P&& path) noexcept;
    
    /// Get current working directory
    /// @return Current working directory path
    [[nodiscard]] Path get_current_directory() noexcept;
    
    /// Set current working directory
    /// @param path New working directory
    /// @return Success or error code
    template<PathType P>
    [[nodiscard]] FSResult<void> set_current_directory(P&& path) noexcept;
    
    /// Get canonical path
    /// @param path Input path
    /// @return Canonical path or error
    template<PathType P>
    [[nodiscard]] FSResult<Path> canonical(P&& path) noexcept;
    
    /// Get relative path
    /// @param path Target path
    /// @param base Base path
    /// @return Relative path or error
    template<PathType P1, PathType P2>
    [[nodiscard]] FSResult<Path> relative(P1&& path, P2&& base) noexcept;
    
    /// Convenience functions for common operations
    namespace Convenience {
        /// Read entire file into memory
        /// @param path File path
        /// @return File contents or error
        template<PathType P>
        [[nodiscard]] FSResult<std::vector<std::byte>> read_file(P&& path) noexcept;
        
        /// Read entire file as string
        /// @param path File path
        /// @return File contents as string or error
        template<PathType P>
        [[nodiscard]] FSResult<std::string> read_text_file(P&& path) noexcept;
        
        /// Write data to file
        /// @param path File path
        /// @param data Data to write
        /// @param overwrite Overwrite if exists
        /// @return Success or error code
        template<PathType P>
        [[nodiscard]] FSResult<void> write_file(
            P&& path,
            std::span<const std::byte> data,
            bool overwrite = true
        ) noexcept;
        
        /// Write string to file
        /// @param path File path
        /// @param text Text to write
        /// @param overwrite Overwrite if exists
        /// @return Success or error code
        template<PathType P>
        [[nodiscard]] FSResult<void> write_text_file(
            P&& path,
            std::string_view text,
            bool overwrite = true
        ) noexcept;
        
        /// Append data to file
        /// @param path File path
        /// @param data Data to append
        /// @return Success or error code
        template<PathType P>
        [[nodiscard]] FSResult<void> append_file(P&& path, std::span<const std::byte> data) noexcept;
        
        /// Append text to file
        /// @param path File path
        /// @param text Text to append
        /// @return Success or error code
        template<PathType P>
        [[nodiscard]] FSResult<void> append_text_file(P&& path, std::string_view text) noexcept;
        
        /// Copy file with progress callback
        /// @param source Source file
        /// @param destination Destination file
        /// @param progress_callback Progress callback function
        /// @return Success or error code
        template<PathType P1, PathType P2>
        [[nodiscard]] FSResult<void> copy_file_with_progress(
            P1&& source,
            P2&& destination,
            std::function<void(std::uint64_t, std::uint64_t)> progress_callback = nullptr
        ) noexcept;
        
        /// Find files matching pattern
        /// @param directory Directory to search
        /// @param pattern Glob pattern
        /// @param recursive Search recursively
        /// @return Vector of matching paths or error
        template<PathType P>
        [[nodiscard]] FSResult<std::vector<Path>> find_files(
            P&& directory,
            std::string_view pattern,
            bool recursive = false
        ) noexcept;
        
        /// Get directory size
        /// @param path Directory path
        /// @param recursive Include subdirectories
        /// @return Total size in bytes or error
        template<PathType P>
        [[nodiscard]] FSResult<std::uint64_t> get_directory_size(P&& path, bool recursive = true) noexcept;
        
        /// Create temporary file
        /// @param prefix Filename prefix
        /// @param suffix Filename suffix
        /// @return Temporary file path or error
        [[nodiscard]] FSResult<Path> create_temporary_file(
            std::string_view prefix = "tmp",
            std::string_view suffix = ""
        ) noexcept;
        
        /// Create temporary directory
        /// @param prefix Directory name prefix
        /// @return Temporary directory path or error
        [[nodiscard]] FSResult<Path> create_temporary_directory(std::string_view prefix = "tmp") noexcept;
    }
    
    /// Advanced filesystem operations
    namespace Advanced {
        /// Watch for file system changes
        class FileWatcher {
        private:
            std::vector<Path> watched_paths_;
            std::atomic<bool> running_{false};
            std::thread watcher_thread_;
            std::function<void(const Path&, FileSystemError)> callback_;
            
        public:
            explicit FileWatcher(std::function<void(const Path&, FileSystemError)> callback) noexcept;
            ~FileWatcher() noexcept;
            
            FSResult<void> add_watch(const Path& path) noexcept;
            FSResult<void> remove_watch(const Path& path) noexcept;
            void start() noexcept;
            void stop() noexcept;
            
        private:
            void watch_loop() noexcept;
        };
        
        /// Memory-mapped file
        class MemoryMappedFile {
        private:
            FileHandle file_;
            std::byte* mapped_memory_;
            std::size_t size_;
            bool read_only_;
            
        public:
            explicit MemoryMappedFile(FileHandle file, bool read_only = true) noexcept;
            ~MemoryMappedFile() noexcept;
            
            // Non-copyable, movable
            MemoryMappedFile(const MemoryMappedFile&) = delete;
            MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;
            MemoryMappedFile(MemoryMappedFile&&) noexcept;
            MemoryMappedFile& operator=(MemoryMappedFile&&) noexcept;
            
            [[nodiscard]] std::span<std::byte> data() noexcept;
            [[nodiscard]] std::span<const std::byte> data() const noexcept;
            [[nodiscard]] std::size_t size() const noexcept { return size_; }
            [[nodiscard]] bool is_read_only() const noexcept { return read_only_; }
            
            FSResult<void> sync() noexcept;
            FSResult<void> resize(std::size_t new_size) noexcept;
        };
        
        /// File lock management
        class FileLock {
        private:
            FileDescriptor fd_;
            bool exclusive_;
            std::atomic<bool> locked_{false};
            
        public:
            explicit FileLock(FileDescriptor fd, bool exclusive = true) noexcept;
            ~FileLock() noexcept;
            
            // Non-copyable, movable
            FileLock(const FileLock&) = delete;
            FileLock& operator=(const FileLock&) = delete;
            FileLock(FileLock&&) noexcept;
            FileLock& operator=(FileLock&&) noexcept;
            
            FSResult<void> lock() noexcept;
            FSResult<void> try_lock() noexcept;
            FSResult<void> unlock() noexcept;
            [[nodiscard]] bool is_locked() const noexcept { return locked_.load(); }
        };
        
        /// Filesystem quota management
        FSResult<void> set_user_quota(UserId uid, std::uint64_t blocks, std::uint64_t inodes) noexcept;
        FSResult<void> set_group_quota(GroupId gid, std::uint64_t blocks, std::uint64_t inodes) noexcept;
        FSResult<std::pair<std::uint64_t, std::uint64_t>> get_user_quota(UserId uid) noexcept;
        FSResult<std::pair<std::uint64_t, std::uint64_t>> get_group_quota(GroupId gid) noexcept;
        
        /// Extended attributes
        FSResult<void> set_extended_attribute(const Path& path, std::string_view name, std::string_view value) noexcept;
        FSResult<std::string> get_extended_attribute(const Path& path, std::string_view name) noexcept;
        FSResult<void> remove_extended_attribute(const Path& path, std::string_view name) noexcept;
        FSResult<std::vector<std::string>> list_extended_attributes(const Path& path) noexcept;
        
        /// Filesystem journaling
        FSResult<void> enable_journaling(const Path& filesystem) noexcept;
        FSResult<void> disable_journaling(const Path& filesystem) noexcept;
        FSResult<void> replay_journal(const Path& filesystem) noexcept;
        FSResult<void> clear_journal(const Path& filesystem) noexcept;
        
        /// Snapshot management
        FSResult<void> create_snapshot(const Path& filesystem, const Path& snapshot_name) noexcept;
        FSResult<void> delete_snapshot(const Path& snapshot) noexcept;
        FSResult<std::vector<Path>> list_snapshots(const Path& filesystem) noexcept;
        FSResult<void> rollback_to_snapshot(const Path& snapshot) noexcept;
    }
    
    /// Debug and introspection functions
    #ifdef DEBUG
    void dump_filesystem_tree(const Path& root = Path("/")) noexcept;
    void dump_inode_table() noexcept;
    void dump_block_allocation() noexcept;
    void validate_filesystem(const Path& filesystem) noexcept;
    void check_filesystem_consistency() noexcept;
    void dump_cache_statistics() noexcept;
    #endif
    
} // namespace FileSystem