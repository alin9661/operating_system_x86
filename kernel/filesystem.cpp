/**
 * @file filesystem.cpp  
 * @brief Modern C++20 filesystem implementation
 * 
 * This file implements a basic in-memory filesystem with modern C++20 features
 * to replace the stub implementation and provide actual file operations.
 */

#include "../include/filesystem.h"
#include "../include/memory.h"
#include <algorithm>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cstring>

namespace FileSystem {
    // Internal filesystem state
    namespace Internal {
        // In-memory inode table
        struct InodeData {
            InodeNumber inode_number;
            FileMetadata metadata;
            std::vector<std::byte> data;
            std::vector<DirectoryEntry> directory_entries; // For directories
            std::string symlink_target; // For symlinks
            std::mutex access_mutex;
            
            InodeData(InodeNumber ino, FileType type) 
                : inode_number(ino), metadata{} {
                metadata.inode = ino;
                metadata.type = type;
                metadata.permissions = FilePermissions::Default;
                metadata.size = 0;
                metadata.blocks = 0;
                metadata.device = 0;
                metadata.owner_uid = 0;
                metadata.owner_gid = 0;
                metadata.hard_links = 1;
                
                auto now = std::chrono::system_clock::now();
                metadata.access_time = now;
                metadata.modify_time = now;
                metadata.change_time = now;
                metadata.birth_time = now;
            }
        };
        
        // Global filesystem state
        static std::unordered_map<InodeNumber, std::unique_ptr<InodeData>> inode_table;
        static std::unordered_map<std::string, InodeNumber> path_to_inode;
        static std::unordered_map<FileDescriptor, std::unique_ptr<FileHandle>> open_files;
        static std::atomic<InodeNumber> next_inode{ROOT_INODE + 1};
        static std::atomic<FileDescriptor> next_fd{3}; // Start after stdin/stdout/stderr
        static std::mutex filesystem_mutex;
        static std::atomic<bool> initialized{false};
        
        // Current working directory
        static std::string current_working_directory{"/"};
        static std::mutex cwd_mutex;
        
        // Filesystem statistics
        static FileSystemStatistics fs_stats;
        
        // Helper functions
        [[nodiscard]] InodeNumber allocate_inode() noexcept {
            return next_inode.fetch_add(1);
        }
        
        [[nodiscard]] FileDescriptor allocate_fd() noexcept {
            return next_fd.fetch_add(1);
        }
        
        [[nodiscard]] InodeData* find_inode_unlocked(InodeNumber inode) noexcept {
            auto it = inode_table.find(inode);
            return (it != inode_table.end()) ? it->second.get() : nullptr;
        }
        
        [[nodiscard]] InodeData* find_inode_by_path_unlocked(const std::string& path) noexcept {
            auto it = path_to_inode.find(path);
            if (it == path_to_inode.end()) {
                return nullptr;
            }
            return find_inode_unlocked(it->second);
        }
        
        // Path resolution
        [[nodiscard]] std::string resolve_path(const std::string& path) noexcept {
            if (path.empty()) return "/";
            
            if (path[0] == '/') {
                // Absolute path
                return path;
            } else {
                // Relative path
                std::lock_guard lock(cwd_mutex);
                std::string resolved = current_working_directory;
                if (resolved != "/" && !path.empty()) {
                    resolved += "/";
                }
                resolved += path;
                return resolved;
            }
        }
        
        // Create directory entry
        void add_directory_entry(InodeData& parent, const std::string& name, InodeNumber child_inode, FileType type) {
            DirectoryEntry entry;
            entry.inode = child_inode;
            entry.name = name;
            entry.type = type;
            entry.record_length = sizeof(DirectoryEntry) + name.length();
            
            parent.directory_entries.push_back(entry);
            parent.metadata.size = parent.directory_entries.size() * sizeof(DirectoryEntry);
            parent.metadata.modify_time = std::chrono::system_clock::now();
        }
        
        // Remove directory entry
        bool remove_directory_entry(InodeData& parent, const std::string& name) {
            auto it = std::find_if(parent.directory_entries.begin(), parent.directory_entries.end(),
                [&name](const DirectoryEntry& entry) { return entry.name == name; });
            
            if (it != parent.directory_entries.end()) {
                parent.directory_entries.erase(it);
                parent.metadata.size = parent.directory_entries.size() * sizeof(DirectoryEntry);
                parent.metadata.modify_time = std::chrono::system_clock::now();
                return true;
            }
            return false;
        }
        
        // Initialize root directory
        void initialize_root_directory() {
            auto root = std::make_unique<InodeData>(ROOT_INODE, FileType::Directory);
            root->metadata.permissions = FilePermissions::Default | FilePermissions::OtherExecute;
            
            // Add . and .. entries
            add_directory_entry(*root, ".", ROOT_INODE, FileType::Directory);
            add_directory_entry(*root, "..", ROOT_INODE, FileType::Directory);
            
            inode_table[ROOT_INODE] = std::move(root);
            path_to_inode["/"] = ROOT_INODE;
        }
    }
    
    // Path class implementation
    Path Path::parent() const noexcept {
        if (path_ == "/" || path_.empty()) {
            return Path("/");
        }
        
        auto pos = path_.find_last_of('/');
        if (pos == std::string::npos) {
            return Path(".");
        }
        if (pos == 0) {
            return Path("/");
        }
        
        return Path(path_.substr(0, pos));
    }
    
    std::string Path::filename() const noexcept {
        if (path_ == "/" || path_.empty()) {
            return "";
        }
        
        auto pos = path_.find_last_of('/');
        if (pos == std::string::npos) {
            return path_;
        }
        
        return path_.substr(pos + 1);
    }
    
    bool Path::is_absolute() const noexcept {
        return !path_.empty() && path_[0] == '/';
    }
    
    bool Path::is_root() const noexcept {
        return path_ == "/";
    }
    
    Path& Path::append(std::string_view component) noexcept {
        if (!component.empty() && component != ".") {
            if (path_ != "/") {
                path_ += "/";
            }
            path_ += component;
            normalize();
        }
        return *this;
    }
    
    Path Path::operator/(std::string_view component) const noexcept {
        Path result(*this);
        return result.append(component);
    }
    
    void Path::normalize() noexcept {
        // Simple path normalization
        // TODO: Handle .. and . properly
        if (path_.empty()) {
            path_ = "/";
        }
        
        // Remove trailing slash unless it's root
        if (path_.length() > 1 && path_.back() == '/') {
            path_.pop_back();
        }
    }
    
    // FileHandle implementation
    FSResult<std::size_t> FileHandle::read(std::span<std::byte> buffer) noexcept {
        if (!is_open_.load()) {
            return compat::unexpected(FileSystemError::InvalidArgument);
        }
        
        if (mode_ == AccessMode::WriteOnly) {
            return compat::unexpected(FileSystemError::PermissionDenied);
        }
        
        std::lock_guard lock(Internal::filesystem_mutex);
        auto* inode_data = Internal::find_inode_by_path_unlocked(path_);
        if (!inode_data) {
            return compat::unexpected(FileSystemError::FileNotFound);
        }
        
        std::lock_guard inode_lock(inode_data->access_mutex);
        
        if (inode_data->metadata.type != FileType::Regular) {
            return compat::unexpected(FileSystemError::UnsupportedOperation);
        }
        
        auto current_pos = position_.load();
        auto available = (current_pos < inode_data->data.size()) ? 
                        (inode_data->data.size() - current_pos) : 0;
        
        auto to_read = std::min(buffer.size(), available);
        if (to_read > 0) {
            std::memcpy(buffer.data(), inode_data->data.data() + current_pos, to_read);
            position_.store(current_pos + to_read);
            inode_data->metadata.access_time = std::chrono::system_clock::now();
        }
        
        Internal::fs_stats.bytes_read.fetch_add(to_read);
        Internal::fs_stats.read_operations.fetch_add(1);
        
        return to_read;
    }
    
    FSResult<std::size_t> FileHandle::write(std::span<const std::byte> data) noexcept {
        if (!is_open_.load()) {
            return compat::unexpected(FileSystemError::InvalidArgument);
        }
        
        if (mode_ == AccessMode::ReadOnly) {
            return compat::unexpected(FileSystemError::PermissionDenied);
        }
        
        std::lock_guard lock(Internal::filesystem_mutex);
        auto* inode_data = Internal::find_inode_by_path_unlocked(path_);
        if (!inode_data) {
            return compat::unexpected(FileSystemError::FileNotFound);
        }
        
        std::lock_guard inode_lock(inode_data->access_mutex);
        
        if (inode_data->metadata.type != FileType::Regular) {
            return compat::unexpected(FileSystemError::UnsupportedOperation);
        }
        
        auto current_pos = position_.load();
        auto required_size = current_pos + data.size();
        
        // Expand file if necessary
        if (required_size > inode_data->data.size()) {
            inode_data->data.resize(required_size);
        }
        
        std::memcpy(inode_data->data.data() + current_pos, data.data(), data.size());
        position_.store(current_pos + data.size());
        
        // Update metadata
        inode_data->metadata.size = inode_data->data.size();
        inode_data->metadata.modify_time = std::chrono::system_clock::now();
        inode_data->metadata.change_time = inode_data->metadata.modify_time;
        
        Internal::fs_stats.bytes_written.fetch_add(data.size());
        Internal::fs_stats.write_operations.fetch_add(1);
        
        return data.size();
    }
    
    FSResult<std::uint64_t> FileHandle::seek(std::int64_t offset, SeekOrigin origin) noexcept {
        if (!is_open_.load()) {
            return compat::unexpected(FileSystemError::InvalidArgument);
        }
        
        std::lock_guard lock(Internal::filesystem_mutex);
        auto* inode_data = Internal::find_inode_by_path_unlocked(path_);
        if (!inode_data) {
            return compat::unexpected(FileSystemError::FileNotFound);
        }
        
        std::uint64_t new_pos;
        auto current_pos = position_.load();
        auto file_size = inode_data->data.size();
        
        switch (origin) {
            case SeekOrigin::Begin:
                new_pos = std::max(static_cast<std::int64_t>(0), offset);
                break;
            case SeekOrigin::Current:
                new_pos = std::max(static_cast<std::int64_t>(0), 
                                  static_cast<std::int64_t>(current_pos) + offset);
                break;
            case SeekOrigin::End:
                new_pos = std::max(static_cast<std::int64_t>(0), 
                                  static_cast<std::int64_t>(file_size) + offset);
                break;
            default:
                return compat::unexpected(FileSystemError::InvalidArgument);
        }
        
        position_.store(new_pos);
        return new_pos;
    }
    
    FSResult<void> FileHandle::close() noexcept {
        if (is_open_.load()) {
            is_open_.store(false);
            
            // Remove from open files table
            std::lock_guard lock(Internal::filesystem_mutex);
            Internal::open_files.erase(fd_);
        }
        return {};
    }
    
    // Core filesystem functions
    
    /// Initialize the file system subsystem
    FSResult<void> init() noexcept {
        if (Internal::initialized.load()) {
            return compat::unexpected(FileSystemError::InvalidArgument);
        }
        
        try {
            std::lock_guard lock(Internal::filesystem_mutex);
            
            // Clear existing state
            Internal::inode_table.clear();
            Internal::path_to_inode.clear();
            Internal::open_files.clear();
            Internal::next_inode = ROOT_INODE + 1;
            Internal::next_fd = 3;
            
            // Initialize filesystem statistics
            Internal::fs_stats = FileSystemStatistics{};
            Internal::fs_stats.total_blocks = 1024; // 4MB filesystem
            Internal::fs_stats.free_blocks = 1024;
            Internal::fs_stats.available_blocks = 1024;
            Internal::fs_stats.total_inodes = INODE_TABLE_SIZE;
            Internal::fs_stats.free_inodes = INODE_TABLE_SIZE - 1; // Root inode used
            Internal::fs_stats.block_size = BLOCK_SIZE;
            Internal::fs_stats.max_filename_length = MAX_FILENAME_LENGTH;
            Internal::fs_stats.type = FileSystemType::ModernFS;
            Internal::fs_stats.mount_point = "/";
            Internal::fs_stats.device_name = "ramfs";
            
            // Create root directory
            Internal::initialize_root_directory();
            
            Internal::initialized = true;
            
        } catch (...) {
            return compat::unexpected(FileSystemError::InsufficientMemory);
        }
        
        return {};
    }
    
    /// Create a file
    template<PathType P>
    FSResult<void> create_file(P&& path, FilePermissions permissions) noexcept {
        if (!Internal::initialized.load()) {
            return compat::unexpected(FileSystemError::InvalidArgument);
        }
        
        std::string resolved_path = Internal::resolve_path(std::string(path));
        
        std::lock_guard lock(Internal::filesystem_mutex);
        
        // Check if file already exists
        if (Internal::find_inode_by_path_unlocked(resolved_path)) {
            return compat::unexpected(FileSystemError::FileExists);
        }
        
        // Find parent directory
        Path parent_path(resolved_path);
        std::string parent_str = parent_path.parent().string();
        auto* parent_inode = Internal::find_inode_by_path_unlocked(parent_str);
        if (!parent_inode || parent_inode->metadata.type != FileType::Directory) {
            return compat::unexpected(FileSystemError::DirectoryNotFound);
        }
        
        // Create new inode
        InodeNumber new_inode = Internal::allocate_inode();
        auto file_data = std::make_unique<Internal::InodeData>(new_inode, FileType::Regular);
        file_data->metadata.permissions = permissions;
        
        // Add to parent directory
        Path path_obj(resolved_path);
        std::string filename = path_obj.filename();
        Internal::add_directory_entry(*parent_inode, filename, new_inode, FileType::Regular);
        
        // Store in filesystem
        Internal::inode_table[new_inode] = std::move(file_data);
        Internal::path_to_inode[resolved_path] = new_inode;
        Internal::fs_stats.free_inodes.fetch_sub(1);
        
        return {};
    }
    
    // Explicit instantiation for common path types
    template FSResult<void> create_file(std::string&& path, FilePermissions permissions) noexcept;
    template FSResult<void> create_file(const std::string& path, FilePermissions permissions) noexcept;
    template FSResult<void> create_file(const char* path, FilePermissions permissions) noexcept;
    
    /// Create a directory
    template<PathType P>
    FSResult<void> create_directory(P&& path, FilePermissions permissions, bool recursive) noexcept {
        if (!Internal::initialized.load()) {
            return compat::unexpected(FileSystemError::InvalidArgument);
        }
        
        std::string resolved_path = Internal::resolve_path(std::string(path));
        
        std::lock_guard lock(Internal::filesystem_mutex);
        
        // Check if directory already exists
        if (Internal::find_inode_by_path_unlocked(resolved_path)) {
            return compat::unexpected(FileSystemError::DirectoryExists);
        }
        
        // Find parent directory
        Path parent_path(resolved_path);
        std::string parent_str = parent_path.parent().string();
        auto* parent_inode = Internal::find_inode_by_path_unlocked(parent_str);
        
        if (!parent_inode) {
            if (recursive) {
                // TODO: Implement recursive directory creation
                return compat::unexpected(FileSystemError::UnsupportedOperation);
            } else {
                return compat::unexpected(FileSystemError::DirectoryNotFound);
            }
        }
        
        if (parent_inode->metadata.type != FileType::Directory) {
            return compat::unexpected(FileSystemError::NotEmpty);
        }
        
        // Create new directory inode
        InodeNumber new_inode = Internal::allocate_inode();
        auto dir_data = std::make_unique<Internal::InodeData>(new_inode, FileType::Directory);
        dir_data->metadata.permissions = permissions | FilePermissions::OtherExecute;
        
        // Add . and .. entries
        Internal::add_directory_entry(*dir_data, ".", new_inode, FileType::Directory);
        Internal::add_directory_entry(*dir_data, "..", parent_inode->inode_number, FileType::Directory);
        
        // Add to parent directory
        Path path_obj(resolved_path);
        std::string dirname = path_obj.filename();
        Internal::add_directory_entry(*parent_inode, dirname, new_inode, FileType::Directory);
        
        // Store in filesystem
        Internal::inode_table[new_inode] = std::move(dir_data);
        Internal::path_to_inode[resolved_path] = new_inode;
        Internal::fs_stats.free_inodes.fetch_sub(1);
        
        return {};
    }
    
    template FSResult<void> create_directory(std::string&& path, FilePermissions permissions, bool recursive) noexcept;
    template FSResult<void> create_directory(const std::string& path, FilePermissions permissions, bool recursive) noexcept;
    template FSResult<void> create_directory(const char* path, FilePermissions permissions, bool recursive) noexcept;
    
    /// Open a file
    template<PathType P>
    FSResult<FileHandle> open(P&& path, AccessMode mode) noexcept {
        if (!Internal::initialized.load()) {
            return compat::unexpected(FileSystemError::InvalidArgument);
        }
        
        std::string resolved_path = Internal::resolve_path(std::string(path));
        
        std::lock_guard lock(Internal::filesystem_mutex);
        
        auto* inode_data = Internal::find_inode_by_path_unlocked(resolved_path);
        if (!inode_data) {
            if (mode == AccessMode::Create || mode == AccessMode::WriteOnly) {
                // Create file if it doesn't exist
                auto create_result = create_file(resolved_path, FilePermissions::Default);
                if (!create_result.has_value()) {
                    return compat::unexpected(create_result.error());
                }
                inode_data = Internal::find_inode_by_path_unlocked(resolved_path);
            } else {
                return compat::unexpected(FileSystemError::FileNotFound);
            }
        }
        
        if (inode_data->metadata.type != FileType::Regular) {
            return compat::unexpected(FileSystemError::UnsupportedOperation);
        }
        
        // Check permissions
        // TODO: Implement proper permission checking
        
        FileDescriptor fd = Internal::allocate_fd();
        auto handle = std::make_unique<FileHandle>(fd, resolved_path, mode);
        
        // Store in open files table
        Internal::open_files[fd] = std::move(handle);
        
        return FileHandle(fd, resolved_path, mode);
    }
    
    template FSResult<FileHandle> open(std::string&& path, AccessMode mode) noexcept;
    template FSResult<FileHandle> open(const std::string& path, AccessMode mode) noexcept;
    template FSResult<FileHandle> open(const char* path, AccessMode mode) noexcept;
    
    /// Check if a path exists
    template<PathType P>
    bool exists(P&& path) noexcept {
        if (!Internal::initialized.load()) {
            return false;
        }
        
        std::string resolved_path = Internal::resolve_path(std::string(path));
        
        std::lock_guard lock(Internal::filesystem_mutex);
        return Internal::find_inode_by_path_unlocked(resolved_path) != nullptr;
    }
    
    template bool exists(std::string&& path) noexcept;
    template bool exists(const std::string& path) noexcept;
    template bool exists(const char* path) noexcept;
    
    /// List directory contents
    template<PathType P>
    FSResult<std::vector<DirectoryEntry>> list_directory(P&& path) noexcept {
        if (!Internal::initialized.load()) {
            return compat::unexpected(FileSystemError::InvalidArgument);
        }
        
        std::string resolved_path = Internal::resolve_path(std::string(path));
        
        std::lock_guard lock(Internal::filesystem_mutex);
        
        auto* inode_data = Internal::find_inode_by_path_unlocked(resolved_path);
        if (!inode_data) {
            return compat::unexpected(FileSystemError::DirectoryNotFound);
        }
        
        if (inode_data->metadata.type != FileType::Directory) {
            return compat::unexpected(FileSystemError::InvalidArgument);
        }
        
        std::lock_guard inode_lock(inode_data->access_mutex);
        return inode_data->directory_entries;
    }
    
    template FSResult<std::vector<DirectoryEntry>> list_directory(std::string&& path) noexcept;
    template FSResult<std::vector<DirectoryEntry>> list_directory(const std::string& path) noexcept;
    template FSResult<std::vector<DirectoryEntry>> list_directory(const char* path) noexcept;
    
    /// Get current working directory
    Path get_current_directory() noexcept {
        std::lock_guard lock(Internal::cwd_mutex);
        return Path(Internal::current_working_directory);
    }
    
    /// Set current working directory
    template<PathType P>
    FSResult<void> set_current_directory(P&& path) noexcept {
        if (!Internal::initialized.load()) {
            return compat::unexpected(FileSystemError::InvalidArgument);
        }
        
        std::string resolved_path = Internal::resolve_path(std::string(path));
        
        std::lock_guard fs_lock(Internal::filesystem_mutex);
        
        auto* inode_data = Internal::find_inode_by_path_unlocked(resolved_path);
        if (!inode_data) {
            return compat::unexpected(FileSystemError::DirectoryNotFound);
        }
        
        if (inode_data->metadata.type != FileType::Directory) {
            return compat::unexpected(FileSystemError::InvalidArgument);
        }
        
        std::lock_guard cwd_lock(Internal::cwd_mutex);
        Internal::current_working_directory = resolved_path;
        
        return {};
    }
    
    template FSResult<void> set_current_directory(std::string&& path) noexcept;
    template FSResult<void> set_current_directory(const std::string& path) noexcept;
    template FSResult<void> set_current_directory(const char* path) noexcept;
    
    /// Convenience functions
    namespace Convenience {
        template<PathType P>
        FSResult<std::string> read_text_file(P&& path) noexcept {
            auto file_result = open(std::forward<P>(path), AccessMode::ReadOnly);
            if (!file_result.has_value()) {
                return compat::unexpected(file_result.error());
            }
            
            auto file = std::move(file_result.value());
            
            // Get file size
            auto metadata_result = file.get_metadata();
            if (!metadata_result.has_value()) {
                return compat::unexpected(metadata_result.error());
            }
            
            auto file_size = metadata_result.value().size;
            std::string content;
            content.resize(file_size);
            
            std::span<std::byte> buffer(reinterpret_cast<std::byte*>(content.data()), file_size);
            auto read_result = file.read(buffer);
            if (!read_result.has_value()) {
                return compat::unexpected(read_result.error());
            }
            
            content.resize(read_result.value());
            return content;
        }
        
        template FSResult<std::string> read_text_file(std::string&& path) noexcept;
        template FSResult<std::string> read_text_file(const std::string& path) noexcept;
        template FSResult<std::string> read_text_file(const char* path) noexcept;
        
        template<PathType P>
        FSResult<void> write_text_file(P&& path, std::string_view text, bool overwrite) noexcept {
            AccessMode mode = overwrite ? AccessMode::Create : AccessMode::WriteOnly;
            
            auto file_result = open(std::forward<P>(path), mode);
            if (!file_result.has_value()) {
                return compat::unexpected(file_result.error());
            }
            
            auto file = std::move(file_result.value());
            
            std::span<const std::byte> data(reinterpret_cast<const std::byte*>(text.data()), text.size());
            auto write_result = file.write(data);
            if (!write_result.has_value()) {
                return compat::unexpected(write_result.error());
            }
            
            return {};
        }
        
        template FSResult<void> write_text_file(std::string&& path, std::string_view text, bool overwrite) noexcept;
        template FSResult<void> write_text_file(const std::string& path, std::string_view text, bool overwrite) noexcept;
        template FSResult<void> write_text_file(const char* path, std::string_view text, bool overwrite) noexcept;
    }
    
} // namespace FileSystem