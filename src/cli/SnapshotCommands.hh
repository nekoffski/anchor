#pragma once

#include "CommandContext.hh"

namespace anchor::cli {

class SnapshotCommands : public kc::NonCopyable, public kc::NonMovable {
   public:
    SnapshotCommands(CLI::App& app, CommandContext& context);

   private:
    Result<void> list();
    Result<void> inspect();
    Result<void> remove();

    CommandContext& m_context;
    Str m_backupName;
    Uuid m_uuid;
};

}  // namespace anchor::cli
