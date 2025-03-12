// This file contains declarations for the main Engine class. You will
// need to add declarations to this file as you develop your Engine.

#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <chrono>

#include "io.hpp"

struct Engine
{
public:
	void accept(ClientConnection conn);

private:
	void connection_thread(ClientConnection conn);
};

// Normally defining a function in a header file causes multiple definition errors if that header is included in multiple .cpp files
// With inline, even if the header is included in multiple cpp files, it does not cause an error
// Inline is mostly used for small and frequently used

// Used for when u wanna include it in multiple .cpp files (include the header in multiple .cpp files)
// Used to avoid function call overhead, let me go through what happens under the hood:

// Without inline, the
// 1. caller function pushes arguments onto the stack
// 2. CPU jumps to the function's address (control transfer) => CPU jumps to execute another block of code, once that function finishes, the control flows back to the caller
// 3. the function executes its code
// 4. the function returns a result
// 5. the caller resumes execution

// with inline,
// the compiler replaces the function call site with the function's body eg

// Normal function call:
// int x = square(5); 
// becomes
// int x = 5 * 5; after inline expansion

// Btw its just a hint to the compiler, not a strict command. Modern compilers often inline small functions automatically when optimisation is on, even without the inline keyword

// If function is large, inlining it will increase binary size, if a function is inline expanded in many places, those instructions might occupy more space in the instruction cache, at runtime
// it could thus lead to more cache misses

// if function is not used much, no point bc wont have function call overhead


inline std::chrono::microseconds::rep getCurrentTimestamp() noexcept
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

#endif
