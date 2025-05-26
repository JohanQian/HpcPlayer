//
// Created by Administrator on 2025/4/12.
//

#include "Source.h"
#include "Message.h"
#include "Log.h"

#define LOG_TAG "Source"

namespace hpc {

void Source::notifyFlagsChanged(uint32_t flags) {
  std::shared_ptr<Message> notify = dupNotify();
  notify->mWhat = kWhatFlagsChanged;
  notify->mArg1 = flags;
  notify->post();
}

void Source::notifyVideoSizeChanged(const std::shared_ptr<Message> &format) {
  std::shared_ptr<Message> notify = dupNotify();
  notify->mWhat = kWhatVideoSizeChanged;
  notify->mObj1 = format.get();
  notify->post();
}

void Source::notifyPrepared(status_t err) {
  ALOGV("Source::notifyPrepared %d", err);
  std::shared_ptr<Message> notify = dupNotify();
  notify->mWhat = kWhatPrepared;
  notify->mArg1 = err;
  notify->post();
}


void Source::notifyInstantiateSecureDecoders(const std::shared_ptr<Message> &reply) {
  std::shared_ptr<Message> notify = dupNotify();
  notify->mWhat = kWhatInstantiateSecureDecoders;
  notify->mObj1 = reply.get();
  notify->post();
}

void Source::onMessageReceived(const std::shared_ptr<Message> & /* msg */) {

}
std::shared_ptr<Message> Source::dupNotify() const {
  return mNotify->dup();
}
} // hpc