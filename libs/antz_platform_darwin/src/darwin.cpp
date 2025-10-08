//
// Created by Kenneth Gulbrandsøy on 26/05/2025.
//

#include "darwin.h"

#include <antz_platform.h>


#include "antz_hal.h"

#include "dsi_framer_ant.hpp"
#include <dsi_serial_generic.hpp>
#include <thread>
#include <error/antz_error.h>
#include <software/ANTFS/antfsmessage.h>

struct antz_hal{
    bool inited{false};
    bool running{false};
    bool destroyed{false};
    DSIFramerANT* pclANT{nullptr};
    DSISerialGeneric* pclSerial{nullptr};
};

#define STARTUP_TIMEOUT_MS 10000L

/**
 * @def ANTZ_SET_DARWIN_ERROR
 * @brief Macro for error reporting from antz_platform darwin implementation.
 *
 * Usage:
 *   ANTZ_SET_DARWIN_ERROR(ERROR_CODE, "Description of what happened");
 *
 * This macro expands to a call to ANTZ_SET_ERROR(), automatically filling source,
 * __FILE__ and __LINE__, so you always know where the error originated.
 */
#define ANTZ_SET_DARWIN_ERROR(code, msg) \
    ANTZ_SET_ERROR((code), "antz_platform_darwin", (msg), __FILE__, __LINE__)


namespace antz_platform {

    antz_hal ctx{};
    
    std::optional<antz_hal_t*> antz_hal_create(const antz_context_init_t* params) {
        if (ctx.inited) return &ctx;

        const auto pclSerial = new DSISerialGeneric();
        if (!pclSerial->Init(params->usb_baud_rate, params->usb_device_number)) {
            delete pclSerial;
            const auto msg = std::string(antz::formatf(
                "Failed to open USB port %d @ baud rate %d",
                params->usb_device_number,
                params->usb_baud_rate
            ));
            ANTZ_SET_DARWIN_ERROR(antz_platform::ErrorCode::SERIAL_OPEN_FAILED, msg);
            // TODO: implement antz_platform_darwin_logging.h
            antz::panicf(msg);
            return std::nullopt;
        }

        const auto pclANT = new DSIFramerANT(pclSerial);
        pclSerial->SetCallback(pclANT);
        if (!pclANT->Init()) {
            delete pclANT;
            delete pclSerial;
            // TODO: implement antz::set_error and ::get_error
            //antz::set_error(ANTZ_ERROR_FRAMER_INIT_FAILED);
            // TODO: implement antz_platform_darwin_logging.h
            /*
            panic("Framer Init failed");
            */
            return std::nullopt;
        }
        if (!pclSerial->Open()) {
            delete pclANT;
            delete pclSerial;
            // TODO: implement antz::set_error and ::get_error
            //antz::set_error(ANTZ_ERROR_SERIAL_OPEN_FAILED);
            // TODO: implement antz_platform_darwin_logging.h
            /*
            panic("Serial Open failed");
            */
            return std::nullopt;
        }

        // Reset current system and wait for startup message or fail on timeout
        pclANT->ResetSystem();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        auto tic = std::chrono::steady_clock::now();
        while (true) {
            if (const USHORT length = pclANT->WaitForMessage(MESSAGE_TIMEOUT); length > 0 && length != DSI_FRAMER_TIMEDOUT) {
                ANT_MESSAGE msg;
                pclANT->GetMessage(&msg);
                if (msg.ucMessageID == MESG_STARTUP_MESG_ID) break;
            }
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - tic).count() > STARTUP_TIMEOUT_MS) {
                // Handle timeout here: error message, cleanup, etc.
                // e.g., log, cleanup, return std::nullopt;
                //antz::set_error(ANTZ_ERROR_STARTUP_MESG_TIMEOUT);
                //panic("Timed out waiting for startup message from ANT device");
                return std::nullopt;
            }
        }

        ctx.pclANT = pclANT;
        ctx.pclSerial = pclSerial;
        ctx.inited = true;

        return &ctx;
    }

    /// Releases all hardware
    void antz_hal_destroy(antz_hal_t* hal){
        if (hal == &ctx && !ctx.destroyed) {
            hal->pclSerial->Close();
            delete hal->pclANT;
            delete hal->pclSerial;
            hal->destroyed = true;
            ctx = {};
        }
    }

    
}
