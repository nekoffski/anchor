#pragma once

#include <future>
#include <kcore/runtime/ThreadSafeQueue.hh>

#include "core/Error.hh"
#include "core/domain/Requests.hh"

namespace anchor {

struct RequestWrapper {
    Request request;
    std::promise<Response> promise;
};

using RequestQueue = kc::ThreadSafeQueue<RequestWrapper>;

class RequestSender {
   public:
    explicit RequestSender(RequestQueue& queue) : m_q(queue) {}

    Result<std::future<Response>> send(Request&& req);

   private:
    RequestQueue& m_q;
};

}  // namespace anchor
