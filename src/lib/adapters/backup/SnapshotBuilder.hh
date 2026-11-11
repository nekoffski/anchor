#pragma once

#include "core/ports/Snapshot.hh"

namespace anchor {

Result<void> validateSnapshotArgs(const SnapshotArgs& args);

class SnapshotWriter : public kc::NonCopyable, public kc::NonMovable {
   public:
    virtual ~SnapshotWriter() = default;
    virtual Result<void> beginEntry(
        const Str& name, const kc::FileInfo& info
    ) = 0;
    virtual Result<void> writeData(std::span<const Byte> data) = 0;
    virtual Result<void> endEntry() = 0;
    virtual Result<void> finish() = 0;
};

class SnapshotBuilder {
   public:
    explicit SnapshotBuilder(const kc::FileSystem& fs);

    Result<SnapshotMetadata> write(
        const SnapshotArgs& args, const kc::Path& output,
        SnapshotWriter& writer, ArchiveType format
    ) const;

   private:
    const kc::FileSystem& m_fs;
};

}  // namespace anchor
