#pragma once

#include "base/runtime/ThreadSafeQueue.hh"
#include "core/domain/Requests.hh"

namespace anchor {

using RequestQueue = ThreadSafeQueue<Request>;

class RequestSender {
   public:
    explicit RequestSender(RequestQueue& queue) : m_q(queue) {}

    bool send(Request&& req) { return m_q.push(std::move(req)); }

   private:
    RequestQueue& m_q;
};

class RequestReceiver {
   public:
    explicit RequestReceiver(RequestQueue& queue) : m_q(queue) {}

    Opt<Request> tryReceive() { return m_q.tryPop(); }

   private:
    RequestQueue& m_q;
};

}  // namespace anchor
