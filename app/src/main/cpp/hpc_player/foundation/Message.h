
#pragma once

#include "Error.h"

#include <condition_variable>
#include <iostream>
#include <list>
#include <mutex>

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

  void setInt(int arg);
  void setObject(void* ptr);

  bool findObject(void** obj) const;
  std::shared_ptr<Message> dup() const;

  status_t post(int64_t delayUs = 0);

  status_t postAndAwaitResponse(std::shared_ptr<Message> *response);

  //bool senderAwaitsResponse(std::shared_ptr<AReplyToken> *replyID);

  status_t postReply(const std::shared_ptr<AReplyToken> &replyID);

  std::weak_ptr<Looper> mLooper;
  std::weak_ptr<Handler> mHandler;
  int mWhat{0};
  int mArg1{0};
  int mArg2{0};
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

class MessageQueue {
 public:
  MessageQueue();
  ~MessageQueue();
  status_t put(int what, int arg1 = 0, int arg2 = 0, void *obj1 = nullptr,
               void *obj2 = nullptr, int obj1_len = 0, int obj2_len = 0);
  status_t remove(int what);
  status_t recycle(std::shared_ptr<Message> &msg);
  status_t start();
  status_t flush();
  status_t abort();
  std::shared_ptr<Message> get(bool block);

 private:
  bool mAbort;
  std::mutex mMutex;
  std::condition_variable mCondition;
  std::list<std::shared_ptr<Message>> mQueue;
  std::list<std::shared_ptr<Message>> mRecycledQueue;
};

}