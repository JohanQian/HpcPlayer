#pragma once

#include <cstdint>
#include <any>
#include <memory>

struct Message {
    uint32_t what = 0;
    int64_t arg1 = 0;
    int32_t arg2 = 0;
    std::shared_ptr<void> obj;
};
