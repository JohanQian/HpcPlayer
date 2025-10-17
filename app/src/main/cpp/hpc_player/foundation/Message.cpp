#include "Message.h"
#include "Log.h"
#include "Looper.h"
#include "Handler.h"

#include <stdlib.h>

#define LOG_TAG "Message"

namespace hpc {

status_t AReplyToken::setReply(const std::shared_ptr<Message> &reply) {
  if (mReplied) {
    ALOGE("trying to post a duplicate reply");
    return -EBUSY;
  }
  if (mReply == nullptr) {
    return ERROR;
  }
  mReply = reply;
  mReplied = true;
  return OK;
}

Message::Message(int what, const std::shared_ptr<Handler> &handler)
  : mWhat(what) {
    setTarget(handler);
}

//Message::Message(int what, int arg1, int arg2, void *obj1, void *obj2,
//                 int obj1_len, int obj2_len)
//    : , mArg1(arg1), mArg2(arg2) {
//  //mTime = CurrentTimeMs();
//  if (obj1 && obj1_len > 0) {
//    mObj1 = malloc(obj1_len * sizeof(uint8_t));
//    if (mObj1) {
//      memcpy(mObj1, obj1, obj1_len);
//    }
//  }
//  if (obj2 && obj2_len > 0) {
//    mObj2 = malloc(obj2_len * sizeof(uint8_t));
//    if (mObj2) {
//      memcpy(mObj2, obj2, obj2_len);
//    }
//  }
//}

void Message::setTarget(const std::shared_ptr<Handler> &handler) {
  if (handler == nullptr) {
    mHandler.lock().reset();
    mLooper.lock().reset();
  } else {
    mHandler = handler->getHandler();
    mLooper = handler->getLooper();
  }
}

void Message::setInt(int64_t arg) {
  mArg1 = arg;
}

void Message::setObject(std::string name ,void* ptr) {
  mObjName = name;
  mObj1 = ptr;
}


status_t Message::post(int64_t delayUs) {
  std::shared_ptr<Looper> looper = mLooper.lock();
  if (looper == nullptr) {
    ALOGW("failed to post message as target looper for handler %d is gone.");
    return -ENOENT;
  }

  looper->post(shared_from_this(), delayUs);
  return OK;
}

status_t Message::postAndAwaitResponse(std::shared_ptr<Message> *response) {
  std::shared_ptr<Looper> looper = mLooper.lock();
  if (looper == nullptr) {
    ALOGW("failed to post message as target looper for handler %d is gone.");
    return -ENOENT;
  }

  std::shared_ptr<AReplyToken> token = looper->createReplyToken();
  if (token == nullptr) {
    ALOGE("failed to create reply token");
    return -ENOMEM;
  }
  mObjName = "replyID";
  mObj1 = token.get();

  looper->post(shared_from_this(), 0 /* delayUs */);
  return looper->awaitResponse(token, *response);
}

status_t Message::postReply(const std::shared_ptr<AReplyToken> &replyToken) {
  if (replyToken == nullptr) {
    ALOGW("failed to post reply to a nullptr token");
    return -ENOENT;
  }
  std::shared_ptr<Looper> looper = mLooper.lock();
  if (looper == nullptr) {
    ALOGW("failed to post reply as target looper is gone.");
    return -ENOENT;
  }
  return looper->postReply(replyToken, shared_from_this());
}

bool Message::senderAwaitsResponse(std::shared_ptr<AReplyToken> *replyToken) {
  void* tmp;
  bool found = findObject("replyID", &tmp);

  if (!found) {
    return false;
  }

  *replyToken = std::shared_ptr<AReplyToken>(static_cast<AReplyToken*>(tmp));
  setObject("replyID", tmp);
  // TODO: delete Object instead of setting it to NULL

  return *replyToken != nullptr;
}

Message::~Message() { clear(); }

void Message::clear() {
  //mWhat = RED_MSG_FLUSH;
  mArg1 = 0;
  mArg2 = 0;
  mTime = 0;
  if (mObj1) {
    free(mObj1);
  }
  if (mObj2) {
    free(mObj2);
  }
}

int Message::what() const {
  return mWhat;
}

bool Message::findObject(const std::string& name, void **obj) const {
  if (mObj1 == nullptr || name != mObjName) {
    return false;
  }
  *obj = mObj1;
  return true;
}

std::shared_ptr<Message> Message::dup() const {
  std::shared_ptr<Message> msg = std::make_shared<Message>();
  if (!msg) {
    return nullptr;
  }
  msg->mWhat = mWhat;
  msg->mArg1 = mArg1;
  msg->mArg2 = mArg1;
  //msg->mTime = CurrentTimeMs();
  if (mObj1 && mObj1_len > 0) {
    msg->mObj1 = malloc(mObj1_len * sizeof(uint8_t));
    if (msg->mObj1) {
      memcpy(msg->mObj1, mObj1, mObj1_len);
    }
  }
  if (mObj2 && mObj2_len > 0) {
    msg->mObj2 = malloc(mObj2_len * sizeof(uint8_t));
    if (msg->mObj2) {
      memcpy(msg->mObj2, mObj2, mObj2_len);
    }
  }
  return msg;
}

}
