#pragma once

#include <future>

#include "base/runtime/ThreadSafeQueue.hh"
#include "core/domain/Requests.hh"

namespace anchor {

struct RequestWrapper {
    Request request;
    std::promise<Response> promise;
};

using RequestQueue = ThreadSafeQueue<RequestWrapper>;

class RequestSender {
   public:
    explicit RequestSender(RequestQueue& queue) : m_q(queue) {}

    Result<std::future<Response>> send(Request&& req);

   private:
    RequestQueue& m_q;
};

}  // namespace anchor
