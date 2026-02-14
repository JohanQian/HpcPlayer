#include "Decoder.h"
#include "common/MediaFormat.h"
#include "renderer/Renderer.h"
#include "common/Message.h"

Decoder::Decoder(const std::shared_ptr<Looper>& looper) : Handler(looper) {}

Decoder::~Decoder() = default;

void Decoder::configure(const std::shared_ptr<MediaFormat>& format) {
    sendMessage({.what = kWhatConfigure, .obj = format});
}

void Decoder::setRenderer(const std::shared_ptr<Renderer>& renderer) {
    sendMessage({.what = kWhatSetRenderer, .obj = renderer});
}

void Decoder::setExtractor(const std::shared_ptr<Extractor>& extractor) {
    sendMessage({.what = kWhatSetExtractor, .obj = extractor});
}

void Decoder::start() {
    sendMessage({kWhatStart});
}

void Decoder::stop() {
    sendMessage({kWhatStop});
}

void Decoder::flush() {
    sendMessage({kWhatFlush});
}

void Decoder::requestInputBuffers() {
    sendMessage({kWhatRequestInputBuffers});
}

std::shared_ptr<MediaFrame> Decoder::getFrame() {
    // Use a timeout to prevent blocking the renderer thread indefinitely.
    // This allows the renderer to process other messages (like Flush) even if no frames are available.
    return frame_queue_.WaitAndPopFor(std::chrono::milliseconds(10)).value_or(nullptr);
}
