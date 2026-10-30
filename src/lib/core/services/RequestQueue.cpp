#include "RequestQueue.hh"

#include "core/Error.hh"

namespace anchor {

Result<std::future<Response>> RequestSender::send(Request&& req) {
    RequestWrapper wrapper{std::move(req)};
    auto future = wrapper.promise.get_future();

    if (not m_q.push(std::move(wrapper))) {
        return Error::unexpected(
            kc::ErrorCode::queueError, "Failed to push request to queue"
        );
    }

    return future;
}

}  // namespace anchor