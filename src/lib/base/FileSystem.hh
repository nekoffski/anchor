#pragma once

#include <string>
#include <vector>

#include "Concepts.hh"
#include "Core.hh"
#include "Error.hh"

namespace anchor {

class Path {
   public:
    template <typename... Args>
        requires std::constructible_from<Str, Args...>
    Path(Args&&... args) : m_path(std::forward<Args>(args)...) {}

    template <typename... Args>
    static Path fmt(const Str& fmt, Args&&... args) {
        return Path(
            fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...)
        );
    }

    const Str& str() const;
    Path parent() const;

    Str filename() const;
    Str filenameWithExtension() const;

    static Path join(const Path& base, const Path& relative);
    static Path cwd();

    Path resolveEnv() const;

    bool endsWith(const Str& suffix) const;
    void append(const Str& suffix);

   private:
    Str m_path;
};

class FileSystem : public NonCopyable, public NonMovable {
   public:
    virtual ~FileSystem() = default;

    virtual bool isFile(const Path& path) const;
    virtual bool isDirectory(const Path& path) const;
    virtual bool exists(const Path& path) const;
    virtual Result<void> copyFile(const Path& from, const Path& to) const;
    virtual Result<void> copyDirectoryRecursive(
        const Path& from, const Path& to
    ) const;

    virtual Result<std::vector<Path>> listFiles(const Path& path) const;
    virtual Result<std::vector<Path>> listDirectories(const Path& path) const;
    virtual Result<void> remove(const Path& path) const;
    virtual Result<void> removeAll(const Path& path) const;
    virtual Result<void> touch(const Path& path) const;
    virtual Result<Path> createDirectory(const Path& path) const;

    virtual Result<void> appendFile(const Path& path, const Str& content) const;
    virtual Result<void> writeFile(const Path& path, const Str& content) const;
    virtual Result<Str> readFile(const Path& path) const;
    virtual Result<std::vector<Str>> readLines(const Path& path) const;
    virtual Result<std::vector<Byte>> readBinary(const Path& path) const;
};

Path operator+(const Path& lhs, const Str& suffix);

}  // namespace anchor
