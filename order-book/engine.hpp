// This file contains declarations for the main Engine class. You will
// need to add declarations to this file as you develop your Engine.

#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <chrono>

#include "io.hpp"
#include <queue>
#include "InstrumentWorker.hpp"

enum class Side {
    BUY,
    SELL
};


struct Engine
{
public:
	void accept(ClientConnection conn);
    void processClientCommand(const ClientCommand& cmd);

// function signatures for more specific order processing

    void processBuyOrder(const ClientCommand& cmd);
    void processSellOrder(const ClientCommand& cmd);
    void processCancelOrder(const ClientCommand& cmd);

private:
	void connection_thread(ClientConnection conn);
    struct Order {
		// uint32_t is used ahead of a plain int because ensures non negative values, guaranteed to be 32 bits on all platforms, can be more memory efficient 
        uint32_t order_id;
        uint32_t price;
        uint32_t quantity;
        std::string instrument;
		bool cancelled = false;
        uint64_t sequence;
		Side side;
		// increasing sequence for order of arrival
    };

    // buy orders (max-heap: higher price first, then earlier sequence)
    struct BuyComparator {
        bool operator()(const Order& a, const Order& b) const {
            if (a.price == b.price)
                return a.sequence > b.sequence;  
            return a.price < b.price;
        }
    };

    // sell orders (min-heap: lower price first, then earlier sequence)
    struct SellComparator {
        bool operator()(const Order& a, const Order& b) const {
            if (a.price == b.price)
                return a.sequence > b.sequence;
            return a.price > b.price;
        }
    };
		
    std::unordered_map<std::string, std::priority_queue<Order, std::vector<Order>, BuyComparator>> buyOrderBooks;
    std::unordered_map<std::string, std::priority_queue<Order, std::vector<Order>, SellComparator>> sellOrderBooks;

	// in cpp cannot have values that are Order& bc references cannot be reassigned 
	// map order id to the order
	std::unordered_map<uint32_t, Order*> orderMap;

	// Value below must be a pointer to begin with because without a pointer, u have to copy the object itself

	// Smart pointers vs Dumb pointers: smart clean up the resources once the obj its pointing at goes out of scope / reassigned

	// Unique pointer is used below because: one owner, the engine

	std::unordered_map<std::string, std::unique_ptr<InstrumentWorker>> instrumentWorkers;
	std::mutex workerMutex;

	InstrumentWorker* getInstrumentWorker(const std::string& instrument);
    
	// sequence number of order
	uint64_t nextSequence = 0;

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
