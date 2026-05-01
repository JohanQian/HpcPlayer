#include "Handler.h"
#include "Message.h"
#include <thread>

namespace hpc {

Handler::Handler(std::shared_ptr<Looper> looper) : looper_(std::move(looper)) {}

void Handler::sendMessage(const Message& msg) {
    auto weak_self = std::weak_ptr<Handler>(shared_from_this());
    looper_->post([weak_self, msg]() {
        if (auto self = weak_self.lock()) {
            self->onMessageReceived(msg);
        }
    });
}

void Handler::sendMessageAndWait(const Message& msg) {
    auto weak_self = std::weak_ptr<Handler>(shared_from_this());
    looper_->postAndWait([weak_self, msg]() {
        if (auto self = weak_self.lock()) {
            self->onMessageReceived(msg);
        }
    });
}

void Handler::sendMessageDelayed(const Message& msg, int64_t delayMs) {
    auto weak_self = std::weak_ptr<Handler>(shared_from_this());
    std::thread([weak_self, msg, delayMs]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        if (auto self = weak_self.lock()) {
            self->looper_->post([weak_self, msg]() {
                if (auto self_inner = weak_self.lock()) {
                    self_inner->onMessageReceived(msg);
                }
            });
        }
    }).detach();
}

Message Handler::obtainMessage(uint32_t what, intptr_t arg1, intptr_t arg2) {
    Message msg;
    msg.what = what;
    msg.arg1 = static_cast<int64_t>(arg1);
    msg.arg2 = static_cast<int32_t>(arg2);
    return msg;
}

} // namespace hpc
