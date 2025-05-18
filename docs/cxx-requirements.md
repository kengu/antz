# C++17 Requirement (`CMAKE_CXX_STANDARD 17`)

This project explicitly requires C++17 as the standard for all builds. 
The main reason for this decision is to leverage modern, robust language 
features that significantly improve the maintainability, safety, and flexibility 
of our codebase.

## Rationale

### 1. Type-Safe Data Structures

We use `std::variant` to represent incoming decoded ANT+ page events. Unlike 
legacy approaches (such as unions or `void*` pointers), `std::variant` provides:

- **Compile-time type safety:** Prevents mismatched casts and type confusion.
- **Extensibility:** Allows pages from new profiles to be added with minimal risk of breakage elsewhere.
- **Modern visitation patterns:** Facilitates cleaner, more robust event dispatch logic.

### 2. Modern Language Features

C++17 brings several improvements that are valuable for embedded and cross-platform development:

- Structured bindings
- `constexpr if` and other compile-time facilities
- Improved template deduction and usability
- Better support and standardization of the STL, including in embedded toolchains

### 3. Toolchain and Platform Support

All the primary platforms and toolchains targeted by this project (including ESP32/ESP-IDF, 
nRF5 SDK with modern GCC, and Android NDK) support C++17 with satisfactory runtime, library, 
and compiler maturity. Using these features does not meaningfully impede our ability to target 
resource-constrained systems, except for a few legacy MCUs (e.g., classic 8-bit AVR), where a 
fallback can be considered if needed.

### 4. Long-Term Maintainability

- Reduces technical debt compared to using manual unions, tag+pointer structures, or custom type erasure.
- Aligns with industry best practices for new C++ projects.

## Summary

Requiring C++17 via `CMAKE_CXX_STANDARD 17` enables the use of safer, more expressive, and 
future-proof constructs—notably `std::variant`—that are key to the architecture and scalability 
of this project.

## Decision

We add `set(CMAKE_CXX_STANDARD 17)` to the top-level for consistent project-wide C++17 enforcement. 

