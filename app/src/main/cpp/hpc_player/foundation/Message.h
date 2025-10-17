
#pragma once

#include "Error.h"

#include <condition_variable>
#include <iostream>
#include <list>
#include <mutex>
#include <any>
#include <memory>

namespace hpc {

class Looper;
class Handler;
struct AReplyToken;

struct Message : public std::enable_shared_from_this<Message>{
  Message() = default;
  explicit Message(int what, const std::shared_ptr<Handler> &handler);
  ~Message();
  void clear();
  int what() const;
  void setTarget(const std::shared_ptr<Handler> &handler);

  void setInt(int64_t arg);
  void setObject(std::string name, void* ptr);

  bool findObject(const std::string& name, void** obj) const;
  std::shared_ptr<Message> dup() const;

  status_t post(int64_t delayUs = 0);

  status_t postAndAwaitResponse(std::shared_ptr<Message> *response);

  bool senderAwaitsResponse(std::shared_ptr<AReplyToken> *replyToken);

  status_t postReply(const std::shared_ptr<AReplyToken> &replyID);

  std::weak_ptr<Looper> mLooper;
  std::weak_ptr<Handler> mHandler;
  int mWhat{0};
  int64_t mArg1{0};
  int64_t mArg2{0};
  std::string mObjName;
  void* mObj1 {nullptr};
  void* mObj2 {nullptr};
  int mObj1_len {0};
  int mObj2_len {0};
  int64_t mTime{0};
};

struct AReplyToken{
  explicit AReplyToken(const std::shared_ptr<Looper> &looper)
      : mLooper(looper),
        mReplied(false) {
  }

  std::shared_ptr<Looper> getLooper() const {
    return mLooper.lock();
  }
  // if reply is not set, returns false; otherwise, it retrieves the reply and returns true
  bool retrieveReply(std::shared_ptr<Message> *reply) {
    if (mReplied) {
      *reply = mReply;
      mReply.reset();
    }
    return mReplied;
  }
  // sets the reply for this token. returns OK or error
  status_t setReply(const std::shared_ptr<Message> &reply);

 private:
  std::weak_ptr<Looper> mLooper;
  std::shared_ptr<Message> mReply;
  bool mReplied;
};

}