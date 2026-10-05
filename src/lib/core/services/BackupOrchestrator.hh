#pragma once

#include "base/Concepts.hh"
#include "base/Config.hh"
#include "base/Core.hh"
#include "base/runtime/Thread.hh"

namespace anchor {

class BackupOrchestrator : public Thread {
   public:
    explicit BackupOrchestrator(const Config& cfg);

    void stop();

   private:
    void run() override;

    Config m_cfg;
};

}  // namespace anchor
