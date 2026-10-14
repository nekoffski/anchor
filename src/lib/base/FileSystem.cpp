#include "FileSystem.hh"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "Env.hh"
#include "Functional.hh"
#include "String.hh"

namespace fs = std::filesystem;

namespace anchor {

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

namespace {

Result<std::vector<Path>> listEntries(const Path& path, fs::file_type type) {
    std::vector<Path> entries;
    std::error_code ec;
    fs::directory_iterator iterator{path.str(), ec};
    const fs::directory_iterator end;
    while (not ec and iterator != end) {
        const auto status = iterator->status(ec);
        if (ec) {
            break;
        }
        if (status.type() == type) {
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
    return listEntries(path, fs::file_type::regular);
}

Result<std::vector<Path>> FileSystem::listDirectories(const Path& path) const {
    return listEntries(path, fs::file_type::directory);
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

Result<Path> FileSystem::createDirectory(const Path& path) const {
    std::error_code ec;
    fs::create_directories(path.str(), ec);
    if (ec) {
        return Error::unexpected(
            ErrorCode::fileSystemError, "Failed to create directory '{}'",
            path.str()
        );
    }
    return path;
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
