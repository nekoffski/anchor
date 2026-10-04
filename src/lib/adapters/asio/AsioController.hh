#pragma once

#include "base/Concepts.hh"
#include "base/Core.hh"
#include "proto/Message.hh"

namespace anchor {

class AsioController : public NonCopyable, public NonMovable {
   public:
    std::unique_ptr<Message> handle(const Message& message);

   private:
};

}  // namespace anchor
