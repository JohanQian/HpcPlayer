    #pragma once

#include <iostream>
#include <memory>
#include <thread>
#include <list>


namespace hpc {

struct Handler;
struct Message;
struct AReplyToken;

struct Looper : public std::enable_shared_from_this<Looper>{
  typedef int32_t event_id;
  typedef int32_t handler_id;

  Looper();
  virtual ~Looper();

  // Takes effect in a subsequent call to start().
  void setName(const char *name);

  handler_id registerHandler(enable_shared_from_this <Handler> handler);
  void unregisterHandler(handler_id handlerID);

  int start();

  int stop();

  static int64_t GetNowUs();

  const char *getName() const {
    return mName.c_str();
  }

 private:
  friend struct Message;       // post()

  struct Event {
    int64_t mWhenUs;
    std::shared_ptr<Message> mMessage;
  };

  std::mutex mLock;
  std::condition_variable mQueueChangedCondition;

  std::string mName;

  std::list<Event> mEventQueue;

  struct LooperThread;
  std::unique_ptr<std::thread> mThread;
  bool mRunning;

  // use a separate lock for reply handling, as it is always on another thread
  // use a central lock, however, to avoid creating a mutex for each reply
  std::mutex mRepliesLock;
  std::condition_variable mRepliesCondition;

  // START --- methods used only by AMessage

  // posts a message on this looper with the given timeout
  void post(const std::shared_ptr<Message> msg, int64_t delayUs);

  // creates a reply token to be used with this looper
  std::shared_ptr<AReplyToken> createReplyToken();
  // waits for a response for the reply token.  If status is OK, the response
  // is stored into the supplied variable.  Otherwise, it is unchanged.
  int awaitResponse(std::shared_ptr<AReplyToken> replyToken, std::shared_ptr<Message> response);
  // posts a reply for a reply token.  If the reply could be successfully posted,
  // it returns OK. Otherwise, it returns an error value.
  int postReply(const std::shared_ptr<AReplyToken> &replyToken, const std::shared_ptr<Message> &msg);

  // END --- methods used only by AMessage

  bool loop();

};

} // namespace android

