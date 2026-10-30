#pragma once

#include <kcore/Concepts.hh>

#include "core/Core.hh"
#include "core/domain/Requests.hh"
#include "core/services/RequestQueue.hh"
#include "proto/Message.hh"

namespace anchor {

class AsioController : public kc::NonCopyable, public kc::NonMovable {
   public:
    explicit AsioController(RequestSender sender);

    std::unique_ptr<proto::Message> handle(const proto::Message& message);

   private:
    RequestSender m_sender;
};

}  // namespace anchor
