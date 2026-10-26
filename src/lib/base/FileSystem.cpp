#include "FileSystem.hh"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <glob/glob.hpp>
#include <ranges>
#include <sstream>

#include "Env.hh"
#include "Functional.hh"
#include "String.hh"

namespace fs = std::filesystem;

namespace anchor {
namespace {

Error fileError(const Str& operation, const Path& path) {
    return Error{
        ErrorCode::fileSystemError, "{} '{}': {}", operation, path.str(),
        std::strerror(errno)
    };
}

class NativeFile : public File {
   public:
    NativeFile(int descriptor, const Path& path)
        : m_descriptor(descriptor), m_path(path) {}
    ~NativeFile() override {
        if (m_descriptor >= 0) {
            ::close(m_descriptor);
        }
    }

    Result<size_t> read(std::span<Byte> buffer) override {
        ssize_t count;
        do {
            count = ::read(m_descriptor, buffer.data(), buffer.size());
        } while (count < 0 && errno == EINTR);
        if (count < 0) {
            return Error::unexpected(fileError("Could not read", m_path));
        }
        return static_cast<size_t>(count);
    }

    Result<void> write(std::span<const Byte> data) override {
        while (not data.empty()) {
            const auto count = ::write(m_descriptor, data.data(), data.size());
            if (count < 0 && errno == EINTR) {
                continue;
            }
            if (count < 0) {
                return Error::unexpected(fileError("Could not write", m_path));
            }
            if (count == 0) {
                return Error::unexpected(
                    ErrorCode::fileSystemError, "No write progress for '{}'",
                    m_path.str()
                );
            }
            data = data.subspan(static_cast<size_t>(count));
        }
        return {};
    }

    Result<void> close() override {
        if (m_descriptor >= 0 &&
            ::close(std::exchange(m_descriptor, -1)) != 0) {
            return Error::unexpected(fileError("Could not close", m_path));
        }
        return {};
    }

   private:
    int m_descriptor;
    Path m_path;
};

}  // namespace

Result<std::unique_ptr<File>> FileSystem::openFile(
    const Path& path, FileMode mode
) const {
    const auto flags = mode == FileMode::read ? O_RDONLY | O_NOFOLLOW
                                              : O_WRONLY | O_CREAT | O_EXCL;
    const auto descriptor = ::open(path.str().c_str(), flags | O_CLOEXEC, 0600);
    if (descriptor < 0) {
        return Error::unexpected(fileError("Could not open", path));
    }
    return std::make_unique<NativeFile>(descriptor, path);
}

Result<FileInfo> FileSystem::info(const Path& path) const {
    struct stat status{};
    if (::lstat(path.str().c_str(), &status) != 0) {
        return Error::unexpected(fileError("Could not stat", path));
    }
    auto type = FileInfo::Type::other;
    if (S_ISREG(status.st_mode)) {
        type = FileInfo::Type::regular;
    } else if (S_ISDIR(status.st_mode)) {
        type = FileInfo::Type::directory;
    } else if (S_ISLNK(status.st_mode)) {
        type = FileInfo::Type::symlink;
    }
#ifdef __APPLE__
    const auto modified = status.st_mtimespec;
#else
    const auto modified = status.st_mtim;
#endif
    FileInfo result{
        .type = type,
        .size = type == FileInfo::Type::regular
                    ? static_cast<u64>(status.st_size)
                    : 0,
        .permissions = static_cast<u32>(status.st_mode & 07777),
        .owner = status.st_uid,
        .group = status.st_gid,
        .modified = std::chrono::sys_time<std::chrono::nanoseconds>{
            std::chrono::seconds{modified.tv_sec} +
            std::chrono::nanoseconds{modified.tv_nsec}
        }
    };
    if (type == FileInfo::Type::symlink) {
        std::error_code ec;
        const auto target = fs::read_symlink(path.str(), ec);
        if (ec) {
            return Error::unexpected(
                ErrorCode::fileSystemError, "Could not read link '{}': {}",
                path.str(), ec.message()
            );
        }
        result.linkTarget = target.string();
    }
    return result;
}

Result<bool> FileSystem::equivalent(
    const Path& first, const Path& second
) const {
    std::error_code ec;
    const auto same = fs::equivalent(first.str(), second.str(), ec);
    if (ec) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Could not compare '{}' and '{}': {}",
            first.str(), second.str(), ec.message()
        );
    }
    return same;
}

bool FileSystem::isFile(const Path& path) const {
    std::error_code ec;
    return fs::is_regular_file(path.str(), ec);
}

bool FileSystem::isDirectory(const Path& path) const {
    std::error_code ec;
    return fs::is_directory(path.str(), ec);
}

bool FileSystem::exists(const Path& path) const {
    std::error_code ec;
    return fs::exists(path.str(), ec);
}

Result<void> FileSystem::copyRecursive(const Path& from, const Path& to) const {
    if (not exists(from)) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Source '{}' does not exist", from.str()
        );
    }

    if (isDirectory(from)) {
        return copyDirectoryRecursive(from, to);
    } else {
        return copyFile(from, to);
    }
}

Result<void> FileSystem::copyFile(const Path& from, const Path& to) const {
    std::error_code ec;
    if (not fs::copy_file(from.str(), to.str(), ec)) {
        return Error::unexpected(
            ErrorCode::fileSystemError,
            "Failed to copy file from '{}' to '{}': {}", from.str(), to.str(),
            ec.message()
        );
    }
    return {};
}

Result<void> FileSystem::copyDirectoryRecursive(
    const Path& from, const Path& to
) const {
    std::error_code ec;
    fs::copy(from.str(), to.str(), fs::copy_options::recursive, ec);
    if (ec) {
        return Error::unexpected(
            ErrorCode::fileSystemError,
            "Failed to copy directory from '{}' to '{}': {}", from.str(),
            to.str(), ec.message()
        );
    }
    return {};
}

Result<u64> FileSystem::fileSize(const Path& path) const {
    if (not isFile(path)) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Path '{}' is not a file", path.str()
        );
    }

    std::error_code ec;
    auto size = fs::file_size(path.str(), ec);
    if (ec) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to get size of '{}': {}",
            path.str(), ec.message()
        );
    }
    return size;
}

std::vector<Path> FileSystem::glob(const Str& pattern) const {
    return glob::glob(pattern) |
           std::views::transform([](const auto& p) { return Path(p); }) |
           std::ranges::to<std::vector<Path>>();
}

namespace {

Result<std::vector<Path>> listEntriesOfType(
    const Path& path, Opt<fs::file_type> type = {}
) {
    std::vector<Path> entries;
    std::error_code ec;
    fs::directory_iterator iterator{path.str(), ec};
    const fs::directory_iterator end;
    while (not ec && iterator != end) {
        const auto matches = not type || iterator->status(ec).type() == *type;
        if (ec) {
            break;
        }
        if (matches) {
            entries.emplace_back(iterator->path().string());
        }
        iterator.increment(ec);
    }
    if (ec) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to list '{}': {}", path.str(),
            ec.message()
        );
    }
    return entries;
}

}  // namespace

Result<std::vector<Path>> FileSystem::listFiles(const Path& path) const {
    return listEntriesOfType(path, fs::file_type::regular);
}

Result<std::vector<Path>> FileSystem::listDirectories(const Path& path) const {
    return listEntriesOfType(path, fs::file_type::directory);
}

Result<std::vector<Path>> FileSystem::listEntries(const Path& path) const {
    return listEntriesOfType(path);
}

Result<void> FileSystem::remove(const Path& path) const {
    std::error_code ec;
    fs::remove(path.str(), ec);
    if (ec) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to remove '{}' - {}",
            path.str(), ec.message()
        );
    }
    return {};
}

Result<void> FileSystem::removeAll(const Path& path) const {
    std::error_code ec;
    fs::remove_all(path.str(), ec);
    if (ec) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to remove all from '{}' - {}",
            path.str(), ec.message()
        );
    }
    return {};
}

Result<void> FileSystem::touch(const Path& path) const {
    std::ofstream file(path.str(), std::ios::app);
    if (not file.is_open()) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to create or open file '{}'",
            path.str()
        );
    }
    return {};
}

Result<void> FileSystem::createDirectory(const Path& path) const {
    std::error_code ec;
    fs::create_directories(path.str(), ec);
    if (ec) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to create directory '{}'",
            path.str()
        );
    }
    return {};
}

Result<void> FileSystem::appendFile(
    const Path& path, const Str& content
) const {
    std::ofstream file(path.str(), std::ios::app);
    if (not file.is_open()) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to open file for appending: {}",
            path.str()
        );
    }
    file << content;
    file.flush();
    if (not file) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to append file: {}", path.str()
        );
    }
    return {};
}

Result<void> FileSystem::writeFile(const Path& path, const Str& content) const {
    std::ofstream file(path.str(), std::ios::trunc);
    if (not file.is_open()) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to open file for writing: {}",
            path.str()
        );
    }
    file << content;
    file.flush();
    if (not file) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to write file: {}", path.str()
        );
    }
    return {};
}

Result<Str> FileSystem::readFile(const Path& path) const {
    std::ifstream file(path.str());
    if (not file.is_open() or isDirectory(path)) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to open file for reading: {}",
            path.str()
        );
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to read file: {}", path.str()
        );
    }
    return buffer.str();
}

Result<std::vector<Str>> FileSystem::readLines(const Path& path) const {
    std::ifstream file(path.str());
    if (not file.is_open() or isDirectory(path)) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to open file for reading: {}",
            path.str()
        );
    }
    std::vector<Str> lines;
    Str line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    if (file.bad()) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to read lines: {}", path.str()
        );
    }
    return lines;
}

Result<std::vector<Byte>> FileSystem::readBinary(const Path& path) const {
    std::ifstream file(path.str(), std::ios::binary | std::ios::ate);
    if (not file.is_open()) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to open file '{}' for reading",
            path.str()
        );
    }
    auto size = file.tellg();
    if (size < 0) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to determine file size: {}",
            path.str()
        );
    }
    file.seekg(0);
    std::vector<Byte> buffer(static_cast<std::size_t>(size) / sizeof(Byte));
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    if (not file) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to read binary file: {}",
            path.str()
        );
    }
    return buffer;
}

const Str& Path::str() const { return m_path; }

Path Path::parent() const {
    auto parentPath = fs::path(m_path).parent_path();
    return Path{parentPath.string()};
}

Str Path::filename() const {
    return fs::path(m_path).filename().replace_extension("").string();
}

Str Path::filenameWithExtension() const {
    return fs::path(m_path).filename().string();
}

Path Path::join(const Path& base, const Path& relative) {
    return Path{fs::path(base.str()) / fs::path(relative.str())};
}

Path Path::cwd() { return Path{fs::current_path().string()}; }

Path Path::lexicallyRelative(const Path& base) const {
    return Path{
        fs::path(m_path).lexically_relative(fs::path(base.str())).string()
    };
}

Path Path::normalized() const {
    return Path{fs::path(m_path).lexically_normal().string()};
}

Result<Path> Path::absolute() const {
    std::error_code ec;
    const auto path = fs::absolute(m_path, ec);
    if (ec) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Could not resolve '{}': {}", m_path,
            ec.message()
        );
    }
    return Path{path.lexically_normal().string()};
}

bool Path::isAbsolute() const { return fs::path(m_path).is_absolute(); }

Path Path::resolveEnv() const {
    auto path = m_path;
    auto parts = split(path, "/");

    for (auto& part : parts) {
        if (part.starts_with("$")) {
            auto envVar = part.substr(1);
            auto envValue = getEnv(envVar).value_or("");
            part = envValue;
        }
    }
    return anchor::join(parts, "/");
}

bool Path::startsWith(const Str& prefix) const {
    return m_path.starts_with(prefix);
}

bool Path::endsWith(const Str& suffix) const {
    if (suffix.size() > m_path.size()) [[unlikely]] {
        return false;
    }
    return std::equal(suffix.rbegin(), suffix.rend(), m_path.rbegin());
}

void Path::append(const Str& suffix) { m_path += suffix; }

Path operator+(const Path& lhs, const Str& suffix) {
    return Path{lhs.str() + suffix};
}

}  // namespace anchor
