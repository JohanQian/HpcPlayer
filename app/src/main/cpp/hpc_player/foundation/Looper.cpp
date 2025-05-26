
#define LOG_TAG "Looper"

#include <chrono>

#include "Looper.h"
#include "Handler.h"
//#include "ALooperRoster.h"
#include "Message.h"
#include "Error.h"

namespace hpc {

// static
int64_t Looper::GetNowUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch())
      .count();
}

Looper::Looper()
    : mRunning(false) {
}

Looper::~Looper() {
  stop();
  // stale AHandlers are now cleaned up in the constructor of the next Looper to come along
}

void Looper::setName(const char *name) {
  mName = name;
}

int Looper::start() {
  mThread = std::make_unique<std::thread>(std::thread([this]() {
    do {
    } while (loop());
  }));
  return OK;
}

int Looper::stop() {
  if (mThread && !mThread->joinable()) {
    return ERROR;
  }
  mRunning = false;
  mThread->join();
  return OK;
}

void Looper::post(const std::shared_ptr<Message> msg, int64_t delayUs) {
  std::lock_guard<std::mutex> lck(mLock);

  int64_t whenUs;
  if (delayUs > 0) {
    int64_t nowUs = GetNowUs();
    whenUs = (delayUs > INT64_MAX - nowUs ? INT64_MAX : nowUs + delayUs);

  } else {
    whenUs = GetNowUs();
  }

  std::list<Event>::iterator it = mEventQueue.begin();
  while (it != mEventQueue.end() && (*it).mWhenUs <= whenUs) {
    ++it;
  }

  Event event;
  event.mWhenUs = whenUs;
  event.mMessage = msg;

  if (it == mEventQueue.begin()) {
    mQueueChangedCondition.notify_all();
  }

  mEventQueue.insert(it, event);
}

bool Looper::loop() {
  Event event;

  {
    std::unique_lock<std::mutex> lck(mLock);
    if (mThread == NULL || !mRunning) {
      return false;
    }
    if (mEventQueue.empty()) {
      mQueueChangedCondition.wait(lck,[this](){return !mEventQueue.empty();});
      return true;
    }
    int64_t whenUs = (*mEventQueue.begin()).mWhenUs;
    int64_t nowUs = GetNowUs();

    if (whenUs > nowUs) {
      int64_t delayUs = whenUs - nowUs;
      if (delayUs > INT64_MAX / 1000) {
        delayUs = INT64_MAX / 1000;
      }
      auto delayTime = std::chrono::system_clock::now() + std::chrono::microseconds(delayUs) ;
      mQueueChangedCondition.wait_until(lck,delayTime,[](){return false;});

      return true;
    }

    event = *mEventQueue.begin();
    mEventQueue.erase(mEventQueue.begin());
  }

  event.mMessage->deliver();
  // NOTE: It's important to note that at ths point our "Looper" object
  // may no longer exist (its final reference may have gone away while
  // delivering the message). We have made sure, however, that loop()
  // won't be called again.

  return true;
}

// to be called by AMessage::postAndAwaitResponse only
std::shared_ptr<AReplyToken> Looper::createReplyToken() {
  return std::make_shared<AReplyToken>(shared_from_this());
}

// to be called by AMessage::postAndAwaitResponse only
int Looper::awaitResponse(std::shared_ptr<AReplyToken> replyToken, std::shared_ptr<Message> response) {
  // return status in case we want to handle an interrupted wait
  std::unique_lock<std::mutex> lck(mRepliesLock);
  if (replyToken == nullptr) {
    return ERROR;
  }
  while (!replyToken->retrieveReply(&response)) {
    {
      std::lock_guard<std::mutex> autoLock(mLock);
      if (mThread == nullptr) {
        return -ENOENT;
      }
    }
    mRepliesCondition.wait(lck,[](){return false;});
  }
  return OK;
}

int Looper::postReply(const std::shared_ptr<AReplyToken> &replyToken, const std::shared_ptr<Message> &reply) {
  std::lock_guard<std::mutex> lck(mRepliesLock);
  int err = replyToken->setReply(reply);
  if (err == OK) {
    mRepliesCondition.notify_all();
  }
  return err;
}

Looper::handler_id Looper::registerHandler(enable_shared_from_this <Handler> handler) {
  return 0;
}

}  // namespace android