#pragma once

#include "Looper.h"
#include <map>

namespace hpc {

struct Message;

struct Handler :public std::enable_shared_from_this<Handler> {
  Handler()
      : mID(0),
        mMessageCounter(0) {
  }
  ~Handler() = default;

  Looper::handler_id id() const {
    return mID;
  }

  std::shared_ptr<Looper> looper() const {
    return mLooper.lock();
  }

  std::weak_ptr<Looper> getLooper() const {
    return mLooper;
  }

  std::weak_ptr<Handler> getHandler() {
    // allow getting a weak reference to a const handler shared_from_this();
    return shared_from_this();
  }

 protected:
  virtual void onMessageReceived(const std::shared_ptr<Message>& msg) = 0;

 private:
  friend struct Message;      // deliverMessage()
  friend struct ALooperRoster; // setID()

  Looper::handler_id mID;
  std::weak_ptr<Looper> mLooper;

  inline void setID(Looper::handler_id id, const std::weak_ptr<Looper>& looper) {
    mID = id;
    mLooper = looper;
  }

  bool mVerboseStats{};
  uint64_t mMessageCounter;
  std::map<uint32_t, uint32_t> mMessages;

  void deliverMessage(const std::shared_ptr<Message>& msg);

  //DISALLOW_EVIL_CONSTRUCTORS(Handler);
};

} // hpc