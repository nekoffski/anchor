#include "FileSystem.hh"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace anchor {

bool FileSystem::isFile(const Path& path) const {
    return fs::is_regular_file(path.str());
}

bool FileSystem::isDirectory(const Path& path) const {
    return fs::is_directory(path.str());
}

bool FileSystem::exists(const Path& path) const {
    return fs::exists(path.str());
}

std::vector<Path> FileSystem::listFiles(const Path& path) const {
    std::vector<Path> files;
    for (const auto& entry : fs::directory_iterator(path.str())) {
        if (entry.is_regular_file()) {
            files.emplace_back(entry.path().string());
        }
    }
    return files;
}

std::vector<Path> FileSystem::listDirectories(const Path& path) const {
    std::vector<Path> directories;
    for (const auto& entry : fs::directory_iterator(path.str())) {
        if (entry.is_directory()) {
            directories.emplace_back(entry.path().string());
        }
    }
    return directories;
}

Result<void> FileSystem::remove(const Path& path) const {
    std::error_code ec;
    fs::remove(path.str(), ec);
    if (ec) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to remove file '{}'", path.str()
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

Result<Path> FileSystem::createDirectory(const Path& path) const {
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
    return {};
}

Result<Str> FileSystem::readFile(const Path& path) const {
    std::ifstream file(path.str());
    if (not file.is_open()) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to open file for reading: {}",
            path.str()
        );
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

Result<std::vector<Str>> FileSystem::readLines(const Path& path) const {
    std::ifstream file(path.str());
    if (not file.is_open()) {
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
    if (size % sizeof(Byte) != 0) {
        return Error::unexpected(
            ErrorCode::invalidArgument,
            "File '{}' size is not a multiple of 1 byte", path.str()
        );
    }
    file.seekg(0);
    std::vector<Byte> buffer(static_cast<std::size_t>(size) / sizeof(Byte));
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return buffer;
}

const Str& Path::str() const { return m_path; }

Path Path::parent() const {
    auto parentPath = fs::path(m_path).parent_path();
    return Path{parentPath.string()};
}

Path Path::join(const Path& base, const Path& relative) {
    return Path{fs::path(base.str()) / fs::path(relative.str())};
}

Path Path::cwd() { return Path{fs::current_path().string()}; }

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
