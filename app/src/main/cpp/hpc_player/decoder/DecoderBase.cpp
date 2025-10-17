
#define LOG_TAG "DecoderBase"

#include "DecoderBase.h"

#include "NuPlayerRenderer.h"

#include <foundation/ADebug.h>
#include <foundation/Message.h>


namespace hpc {

DecoderBase::DecoderBase(const std::shared_ptr<Message> &notify)
    :  mNotify(notify),
       mBufferGeneration(0),
       mPaused(false),
       mStats(std::make_shared<Message>()),
       mRequestInputBuffersPending(false) {
  // Every decoder has its own looper because MediaCodec operations
  // are blocking, but NuPlayer needs asynchronous operations.
  mDecoderLooper = new Looper;
  mDecoderLooper->setName("NPDecoder");
  mDecoderLooper->start(false, false, ANDROID_PRIORITY_AUDIO);
}

DecoderBase::~DecoderBase() {
  stopLooper();
}

static
status_t PostAndAwaitResponse(
    const std::shared_ptr<Message> &msg, std::shared_ptr<Message> *response) {
  status_t err = msg->postAndAwaitResponse(response);

  if (err != OK) {
    return err;
  }

  if (!(*response)->findInt32("err", &err)) {
    err = OK;
  }

  return err;
}

void DecoderBase::configure(const std::shared_ptr<Message> &format) {
  std::shared_ptr<Message> msg = new Message(kWhatConfigure, this);
  msg->setMessage("format", format);
  msg->post();
}

void DecoderBase::init() {
  mDecoderLooper->registerHandler(this);
}

void DecoderBase::stopLooper() {
  mDecoderLooper->unregisterHandler(id());
  mDecoderLooper->stop();
}

void DecoderBase::setParameters(const std::shared_ptr<Message> &params) {
  std::shared_ptr<Message> msg = new Message(kWhatSetParameters, this);
  msg->setMessage("params", params);
  msg->post();
}

void DecoderBase::setRenderer(const sp<Renderer> &renderer) {
  std::shared_ptr<Message> msg = new Message(kWhatSetRenderer, this);
  msg->setObject("renderer", renderer);
  msg->post();
}

void DecoderBase::pause() {
  std::shared_ptr<Message> msg = new Message(kWhatPause, this);

  std::shared_ptr<Message> response;
  PostAndAwaitResponse(msg, &response);
}

void DecoderBase::signalFlush() {
  (new Message(kWhatFlush, this))->post();
}

void DecoderBase::signalResume(bool notifyComplete) {
  std::shared_ptr<Message> msg = new Message(kWhatResume, this);
  msg->setInt32("notifyComplete", notifyComplete);
  msg->post();
}

void DecoderBase::initiateShutdown() {
  (new Message(kWhatShutdown, this))->post();
}

void DecoderBase::onRequestInputBuffers() {
  if (mRequestInputBuffersPending) {
    return;
  }

  // doRequestBuffers() return true if we should request more data
  if (doRequestBuffers()) {
    mRequestInputBuffersPending = true;

    std::shared_ptr<Message> msg = new Message(kWhatRequestInputBuffers, this);
    msg->post(10 * 1000LL);
  }
}

void DecoderBase::onMessageReceived(const std::shared_ptr<Message> &msg) {

  switch (msg->what()) {
    case kWhatConfigure:
    {
      std::shared_ptr<Message> format;
      CHECK(msg->findMessage("format", &format));
        (format);
      break;
    }

    case kWhatSetParameters:
    {
      std::shared_ptr<Message> params;
      CHECK(msg->findMessage("params", &params));
      onSetParameters(params);
      break;
    }

    case kWhatSetRenderer:
    {
      sp<RefBase> obj;
      CHECK(msg->findObject("renderer", &obj));
      onSetRenderer(static_cast<Renderer *>(obj.get()));
      break;
    }

    case kWhatPause:
    {
      std::shared_ptr<ReplyToken> replyID;
      CHECK(msg->senderAwaitsResponse(&replyID));

      mPaused = true;

      (new Message)->postReply(replyID);
      break;
    }

    case kWhatRequestInputBuffers:
    {
      mRequestInputBuffersPending = false;
      onRequestInputBuffers();
      break;
    }

    case kWhatFlush:
    {
      onFlush();
      break;
    }

    case kWhatResume:
    {
      int32_t notifyComplete;
      CHECK(msg->findInt32("notifyComplete", &notifyComplete));

      onResume(notifyComplete);
      break;
    }

    case kWhatShutdown:
    {
      onShutdown(true);
      break;
    }

    default:
      break;
  }
}

void DecoderBase::handleError(int32_t err)
{
  // We cannot immediately release the codec due to buffers still outstanding
  // in the renderer.  We signal to the player the error so it can shutdown/release the
  // decoder after flushing and increment the generation to discard unnecessary messages.

  ++mBufferGeneration;

  std::shared_ptr<Message> notify = mNotify->dup();
  notify->setInt32("what", kWhatError);
  notify->setInt32("err", err);
  notify->post();
}

}  // namespace android

