/**
 * @file mock_file_system.h
 * @brief Mock classes for file system operations
 *
 * Provides Google Mock implementations of file system interfaces
 * for isolated unit testing without real file I/O dependencies.
 */

#ifndef FFVOICE_TESTS_MOCKS_MOCK_FILE_SYSTEM_H
#define FFVOICE_TESTS_MOCKS_MOCK_FILE_SYSTEM_H

#include <gmock/gmock.h>
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <map>

namespace ffvoice {
namespace test {

/**
 * @brief File metadata structure
 */
struct FileMetadata {
    std::string path;
    size_t size;
    bool exists;
    bool is_readable;
    bool is_writable;
    uint64_t last_modified;
};

/**
 * @class IFileReader
 * @brief Interface for file reading operations
 */
class IFileReader {
public:
    virtual ~IFileReader() = default;

    /**
     * @brief Open file for reading
     *
     * @param path File path
     * @return True if file opened successfully
     */
    virtual bool Open(const std::string& path) = 0;

    /**
     * @brief Close file
     */
    virtual void Close() = 0;

    /**
     * @brief Check if file is open
     *
     * @return True if file is open
     */
    virtual bool IsOpen() const = 0;

    /**
     * @brief Read data from file
     *
     * @param buffer Buffer to store read data
     * @param size Number of bytes to read
     * @return Number of bytes actually read
     */
    virtual size_t Read(void* buffer, size_t size) = 0;

    /**
     * @brief Seek to position in file
     *
     * @param offset Offset in bytes
     * @param origin Origin (SEEK_SET, SEEK_CUR, SEEK_END)
     * @return True if seek successful
     */
    virtual bool Seek(int64_t offset, int origin) = 0;

    /**
     * @brief Get current position in file
     *
     * @return Current position in bytes
     */
    virtual int64_t Tell() const = 0;

    /**
     * @brief Get file size
     *
     * @return File size in bytes
     */
    virtual size_t Size() const = 0;
};

/**
 * @class MockFileReader
 * @brief Mock implementation of file reader
 */
class MockFileReader : public IFileReader {
public:
    MOCK_METHOD(bool, Open, (const std::string& path), (override));
    MOCK_METHOD(void, Close, (), (override));
    MOCK_METHOD(bool, IsOpen, (), (const, override));
    MOCK_METHOD(size_t, Read, (void* buffer, size_t size), (override));
    MOCK_METHOD(bool, Seek, (int64_t offset, int origin), (override));
    MOCK_METHOD(int64_t, Tell, (), (const, override));
    MOCK_METHOD(size_t, Size, (), (const, override));

    /**
     * @brief Simulate file with pre-defined content
     *
     * @param content File content to simulate
     */
    void SimulateFileContent(const std::vector<uint8_t>& content) {
        simulated_content_ = content;
        file_position_ = 0;

        using ::testing::_;
        using ::testing::Return;
        using ::testing::Invoke;

        ON_CALL(*this, Open(_)).WillByDefault(Return(true));
        ON_CALL(*this, IsOpen()).WillByDefault(Return(true));
        ON_CALL(*this, Size()).WillByDefault(Return(content.size()));

        ON_CALL(*this, Read(_, _))
            .WillByDefault(Invoke([this](void* buffer, size_t size) -> size_t {
                size_t bytes_to_read = std::min(
                    size,
                    simulated_content_.size() - file_position_
                );

                if (bytes_to_read > 0) {
                    std::memcpy(
                        buffer,
                        &simulated_content_[file_position_],
                        bytes_to_read
                    );
                    file_position_ += bytes_to_read;
                }

                return bytes_to_read;
            }));

        ON_CALL(*this, Seek(_, _))
            .WillByDefault(Invoke([this](int64_t offset, int origin) -> bool {
                int64_t new_pos = 0;

                switch (origin) {
                    case SEEK_SET:
                        new_pos = offset;
                        break;
                    case SEEK_CUR:
                        new_pos = file_position_ + offset;
                        break;
                    case SEEK_END:
                        new_pos = simulated_content_.size() + offset;
                        break;
                    default:
                        return false;
                }

                if (new_pos < 0 || new_pos > static_cast<int64_t>(simulated_content_.size())) {
                    return false;
                }

                file_position_ = static_cast<size_t>(new_pos);
                return true;
            }));

        ON_CALL(*this, Tell())
            .WillByDefault(Invoke([this]() -> int64_t {
                return static_cast<int64_t>(file_position_);
            }));
    }

private:
    std::vector<uint8_t> simulated_content_;
    size_t file_position_ = 0;
};

/**
 * @class IFileWriter
 * @brief Interface for file writing operations
 */
class IFileWriter {
public:
    virtual ~IFileWriter() = default;

    /**
     * @brief Open file for writing
     *
     * @param path File path
     * @param append Append mode (true) or overwrite mode (false)
     * @return True if file opened successfully
     */
    virtual bool Open(const std::string& path, bool append = false) = 0;

    /**
     * @brief Close file
     */
    virtual void Close() = 0;

    /**
     * @brief Check if file is open
     *
     * @return True if file is open
     */
    virtual bool IsOpen() const = 0;

    /**
     * @brief Write data to file
     *
     * @param buffer Buffer containing data to write
     * @param size Number of bytes to write
     * @return Number of bytes actually written
     */
    virtual size_t Write(const void* buffer, size_t size) = 0;

    /**
     * @brief Flush buffered data to disk
     *
     * @return True if flush successful
     */
    virtual bool Flush() = 0;
};

/**
 * @class MockFileWriter
 * @brief Mock implementation of file writer
 */
class MockFileWriter : public IFileWriter {
public:
    MOCK_METHOD(bool, Open, (const std::string& path, bool append), (override));
    MOCK_METHOD(void, Close, (), (override));
    MOCK_METHOD(bool, IsOpen, (), (const, override));
    MOCK_METHOD(size_t, Write, (const void* buffer, size_t size), (override));
    MOCK_METHOD(bool, Flush, (), (override));

    /**
     * @brief Capture written data for verification
     *
     * @return Vector of data written to file
     */
    const std::vector<uint8_t>& GetWrittenData() const {
        return written_data_;
    }

    /**
     * @brief Clear captured data
     */
    void ClearWrittenData() {
        written_data_.clear();
    }

    /**
     * @brief Set up mock to capture written data
     */
    void CaptureWrittenData() {
        using ::testing::_;
        using ::testing::Return;
        using ::testing::Invoke;

        ON_CALL(*this, Open(_, _)).WillByDefault(Return(true));
        ON_CALL(*this, IsOpen()).WillByDefault(Return(true));
        ON_CALL(*this, Flush()).WillByDefault(Return(true));

        ON_CALL(*this, Write(_, _))
            .WillByDefault(Invoke([this](const void* buffer, size_t size) -> size_t {
                const uint8_t* data = static_cast<const uint8_t*>(buffer);
                written_data_.insert(written_data_.end(), data, data + size);
                return size;
            }));
    }

private:
    std::vector<uint8_t> written_data_;
};

/**
 * @class IFileSystem
 * @brief Interface for file system operations
 */
class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    /**
     * @brief Check if file exists
     *
     * @param path File path
     * @return True if file exists
     */
    virtual bool FileExists(const std::string& path) const = 0;

    /**
     * @brief Check if directory exists
     *
     * @param path Directory path
     * @return True if directory exists
     */
    virtual bool DirectoryExists(const std::string& path) const = 0;

    /**
     * @brief Get file metadata
     *
     * @param path File path
     * @return File metadata structure
     */
    virtual FileMetadata GetFileMetadata(const std::string& path) const = 0;

    /**
     * @brief Create directory
     *
     * @param path Directory path
     * @return True if directory created successfully
     */
    virtual bool CreateDirectory(const std::string& path) = 0;

    /**
     * @brief Delete file
     *
     * @param path File path
     * @return True if file deleted successfully
     */
    virtual bool DeleteFile(const std::string& path) = 0;

    /**
     * @brief Delete directory
     *
     * @param path Directory path
     * @param recursive Delete recursively
     * @return True if directory deleted successfully
     */
    virtual bool DeleteDirectory(const std::string& path, bool recursive = false) = 0;

    /**
     * @brief List files in directory
     *
     * @param path Directory path
     * @param pattern File pattern filter (e.g., "*.wav")
     * @return Vector of file paths
     */
    virtual std::vector<std::string> ListFiles(
        const std::string& path,
        const std::string& pattern = "*"
    ) const = 0;

    /**
     * @brief Create file reader
     *
     * @return Unique pointer to file reader
     */
    virtual std::unique_ptr<IFileReader> CreateReader() = 0;

    /**
     * @brief Create file writer
     *
     * @return Unique pointer to file writer
     */
    virtual std::unique_ptr<IFileWriter> CreateWriter() = 0;
};

/**
 * @class MockFileSystem
 * @brief Mock implementation of file system
 */
class MockFileSystem : public IFileSystem {
public:
    MOCK_METHOD(bool, FileExists, (const std::string& path), (const, override));
    MOCK_METHOD(bool, DirectoryExists, (const std::string& path), (const, override));
    MOCK_METHOD(FileMetadata, GetFileMetadata, (const std::string& path), (const, override));
    MOCK_METHOD(bool, CreateDirectory, (const std::string& path), (override));
    MOCK_METHOD(bool, DeleteFile, (const std::string& path), (override));
    MOCK_METHOD(bool, DeleteDirectory, (const std::string& path, bool recursive), (override));
    MOCK_METHOD(
        std::vector<std::string>,
        ListFiles,
        (const std::string& path, const std::string& pattern),
        (const, override)
    );
    MOCK_METHOD(std::unique_ptr<IFileReader>, CreateReader, (), (override));
    MOCK_METHOD(std::unique_ptr<IFileWriter>, CreateWriter, (), (override));

    /**
     * @brief Set up in-memory virtual file system
     */
    void SetupVirtualFileSystem() {
        using ::testing::_;
        using ::testing::Return;
        using ::testing::Invoke;

        ON_CALL(*this, FileExists(_))
            .WillByDefault(Invoke([this](const std::string& path) -> bool {
                return virtual_files_.find(path) != virtual_files_.end();
            }));

        ON_CALL(*this, DirectoryExists(_))
            .WillByDefault(Invoke([this](const std::string& path) -> bool {
                return virtual_directories_.find(path) != virtual_directories_.end();
            }));

        ON_CALL(*this, CreateDirectory(_))
            .WillByDefault(Invoke([this](const std::string& path) -> bool {
                virtual_directories_.insert(path);
                return true;
            }));

        ON_CALL(*this, DeleteFile(_))
            .WillByDefault(Invoke([this](const std::string& path) -> bool {
                return virtual_files_.erase(path) > 0;
            }));
    }

    /**
     * @brief Add virtual file to mock file system
     *
     * @param path File path
     * @param content File content
     */
    void AddVirtualFile(const std::string& path, const std::vector<uint8_t>& content) {
        virtual_files_[path] = content;
    }

    /**
     * @brief Add virtual directory to mock file system
     *
     * @param path Directory path
     */
    void AddVirtualDirectory(const std::string& path) {
        virtual_directories_.insert(path);
    }

    /**
     * @brief Get content of virtual file
     *
     * @param path File path
     * @return File content (empty if not found)
     */
    std::vector<uint8_t> GetVirtualFileContent(const std::string& path) const {
        auto it = virtual_files_.find(path);
        return (it != virtual_files_.end()) ? it->second : std::vector<uint8_t>();
    }

private:
    std::map<std::string, std::vector<uint8_t>> virtual_files_;
    std::set<std::string> virtual_directories_;
};

} // namespace test
} // namespace ffvoice

#endif // FFVOICE_TESTS_MOCKS_MOCK_FILE_SYSTEM_H
