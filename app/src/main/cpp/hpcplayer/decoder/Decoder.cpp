#include "Decoder.h"
#include "common/MediaFormat.h"
#include "renderer/Renderer.h"
#include "common/Message.h"

namespace hpc {

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
    sendMessageAndWait({kWhatStop});
}

void Decoder::flush() {
    sendMessage({kWhatFlush});
}

void Decoder::requestInputBuffers() {
    sendMessage({kWhatRequestInputBuffers});
}

} // namespace hpc
