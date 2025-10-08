//
// Created by Kenneth Gulbrandsøy on 25/05/2025.
//

#include "antz_platform.h"

#include <antz_context.h>
#include <antz_hal.h>
#include <vector>

struct antz_channel {
    antz_hal_channel_t* hal;
};

struct antz_context {
    antz_hal_t* hal;
    std::vector<antz_channel> channels;
};

namespace antz {
    std::optional<antz_context_t> init(const antz_context_init_t* params) {
        if (const auto hal = antz_platform::antz_hal_create(params)) {
            antz_context ctx{};
            ctx.hal = hal.value();
            return ctx;
        }
        return std::nullopt;
    }

    std::optional<antz_channel> open(antz_context_t* ctx, const antz_channel_config_t* cfg) {
        if (const auto hal = antz_platform::antz_hal_open(ctx->hal, cfg)) {
            antz_channel channel{};
            channel.hal = hal.value();
            ctx->channels.push_back(channel);
            return channel;
        }
        return std::nullopt;
    }

    void close(antz_context_t* ctx, const antz_channel* channel) {
        // Remove a channel from the vector
        const auto it = std::find_if(ctx->channels.begin(), ctx->channels.end(),
           [channel](const antz_channel& test) {
               return test.hal == channel->hal;
           }
        );

        if (it != ctx->channels.end()) {
            antz_platform::antz_hal_close(ctx->hal, channel->hal);
            ctx->channels.erase(it);
        }
    }

    bool start(const antz_context_t* ctx) {
        return antz_platform::antz_hal_start(ctx->hal);
    }

    void stop(const antz_context_t* ctx) {
        antz_platform::antz_hal_stop(ctx->hal);
    }

    void shutdown(antz_context_t* ctx) {
        // Close all open channels
        while (!ctx->channels.empty()) {
            close(ctx, &ctx->channels.back());
        }
        antz_platform::antz_hal_destroy(ctx->hal);
    }
}
