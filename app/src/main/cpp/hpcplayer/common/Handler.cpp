#include "Handler.h"
#include "Looper.h"
#include "Message.h"

Handler::Handler(std::shared_ptr<Looper> looper) : looper_(std::move(looper)) {}

void Handler::sendMessage(const Message& msg) {
    if (looper_) {
        auto self = shared_from_this();
        looper_->post([self, msg]() {
            self->onMessageReceived(msg);
        });
    }
}

Message Handler::obtainMessage(uint32_t what, intptr_t arg1, intptr_t arg2) {
    Message msg;
    msg.what = what;
    msg.arg1 = arg1;
    msg.arg2 = arg2;
    return msg;
}
