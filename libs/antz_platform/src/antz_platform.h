//
// Created by Kenneth Gulbrandsøy on 25/05/2025.
//

#pragma once

namespace antz_platform
{
    /**
     * @enum ErrorCode
     * @brief Represents recoverable and unrecoverable errors in Antz platform implementations (HALs).
     *
     * Range: 1100–1199 (reserved for antz_platform)
     */
    enum ErrorCode {
        OK                       = 0,    ///< Success, no error.

        USB_PORT_OPEN_FAILED     = 1101, ///< Failed to open USB serial port (DSISerialGeneric::Init).
        ANT_SYSTEM_INIT_FAILED   = 1102, ///< Failed to initialize ANT+ system.
        SERIAL_OPEN_FAILED       = 1103, ///< Failed to open serial device for ANT+ communication.
        STARTUP_MESSAGE_TIMEOUT  = 1104, ///< Timed out waiting for startup message from device.
        INVALID_STATE            = 1105, ///< Initialization called when already initialized.
        RESOURCE_ALLOCATION_FAIL = 1106, ///< Failed to allocate DSIFramerANT or DSISerialGeneric.

        UNKNOWN                  = 1199  ///< Unknown or uncategorized error.
    };

}