#pragma once

#include <functional>
#include <memory>
#include <string>
#include "Looper.h"

struct Message;

class Handler : public std::enable_shared_from_this<Handler> {
public:
    explicit Handler(std::shared_ptr<Looper> looper);
    virtual ~Handler() = default;

    void sendMessage(const Message& msg);
    void sendMessageDelayed(const Message& msg, int64_t delayMs);
    Message obtainMessage(uint32_t what = 0, intptr_t arg1 = 0, intptr_t arg2 = 0);

protected:
    virtual void onMessageReceived(const Message& msg) = 0;
    
    std::shared_ptr<Looper> looper_;
};
