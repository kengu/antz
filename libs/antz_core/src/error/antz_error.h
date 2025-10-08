//
// Created by Kenneth Gulbrandsøy on 01/07/2025.
//

#pragma once

#include <logger/antz_logger.h>
#include <string>

/**
 * @file antz_error.h
 * @brief Standard error handling definitions for the antz_core module.
 *
 * This file provides error codes, error containers, reporting functions, and utilities
 * to ensure consistent error handling throughout the codebase.
 */
namespace antz {

    /**
     * @struct Error
     * @brief Represents a detailed error state.
     *
     * This structure carries:
     *  - The specific error code
     *  - A string indicating which module reported the error
     *  - A human-readable message with extra diagnostics
     */
    struct Error {
        int code = 0;
        std::string source;      // Module name, e.g. "antz_core", "antz_platform"
        std::string message;     // Optional, for diagnostics
        std::string file;        // Optional, for diagnostics
        int line = 0;            // Optional, for diagnostics

        /**
         * @brief Constructs a new Error object.
         * @param c The error code (default is OK)
         * @param s The error source/module (default is "antz_core")
         * @param msg Diagnostic, message (optional)
         * @param file Diagnostic, source file (optional)
         * @param line Diagnostic, source file line (optional)
         */
        explicit Error(
            const int c = 0,
            const std::string& s = "antz_core",
            const std::string& msg = "",
            const std::string& file = "",
            const int line = 0)
            : code(c), source(s), message(msg), file(file), line(line) {}

    };

    /**
     * @brief Records a new error in the system.
     *
     * Call this function when an error condition is detected. Use the macro below to
     * automatically fill the file and line number for convenience.
     *
     * @param code      Numeric error code (can come from any module)
     * @param source    Short string for originating module or context
     * @param message   Human-readable error description
     * @param file      Name of the file where the error occurred (use __FILE__)
     * @param line      Line number where the error occurred (use __LINE__)
     */
    void set_error(const int code,
                   const std::string& source,
                   const std::string& message,
                   const std::string& file,
                   const int line);

    /**
     * @def ANTZ_SET_ERROR
     * @brief Macro for error reporting with automatic file and line number.
     *
     * Usage:
     *   ANTZ_SET_ERROR(ERROR_CODE, "module_name", "Description of what happened");
     *
     * This macro expands to a call to set_error(), automatically filling __FILE__ and __LINE__,
     * so you always know where the error originated.
     */
    #define ANTZ_SET_ERROR(code, source, msg) antz::set_error((code), (source), (msg), __FILE__, __LINE__)

    /**
     * @brief Returns the most recently set error.
     *
     * Use this function to inspect why an operation failed after you detect an error.
     * The resulting Error object includes code, module, and context message.
     *
     * @return The last error reported via set_error.
     */
    Error get_error();

    /**
     * @brief Logs an Error object using the antz logger infrastructure.
     *
     * This function formats the contents of a given antz::Error object
     * and writes it to the current logger at the specified log level.
     * The log message includes the error code, source module, descriptive message,
     * and optional diagnostic information such as the file and line where the error
     * was reported.
     *
     * @param error    The Error object containing detailed error information to log.
     * @param level  The log severity level to use (e.g., Warn, Panic, Info, etc.).
     *
     * @note This utility ensures all relevant context about errors is captured in logs,
     *       aiding debugging and system monitoring.
     *
     * @see antz::Error
     * @see LogLevel
     */
    void log_error(const Error& error, antz::LogLevel level);

}
