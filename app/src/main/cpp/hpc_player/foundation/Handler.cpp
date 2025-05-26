//
// Created by Administrator on 2025/4/7.
//

#include "Handler.h"

namespace hpc {
void Handler::deliverMessage(const std::shared_ptr<Message> &msg) {
  onMessageReceived(msg);
  mMessageCounter++;
}
} // hpc
