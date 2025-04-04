#pragma once
#include "io.hpp" 
#include <thread>
#include <atomic>
#include <string>
#include "ThreadSafeQueue.hpp"

struct Engine;

struct InstrumentWorker {
    ThreadSafeQueue<ClientCommand> buyQueue;

    ThreadSafeQueue<ClientCommand> sellQueue;
    
    std::thread buyWorkerThread;

    std::thread sellWorkerThread;
    
    // Signal to stop the worker.
    // If stop was not atomic, when main thread sets stop = true to signal shtdown, compiler or CPU might optimise the worker thread's loop by caching the value of stop in a register
    // However, if atomic (sequential consistency by default), every write to it is immeditael visible to all threads, and compiler also does not reorder
    

    // We dont need a seperate condition variable here bc this isnt a wait until "stop" operation
    // this is a one time thing, only for a shutdown operation
    std::atomic<bool> stop {false};
    
    std::string instrument;

    InstrumentWorker(const std::string& instr);
    
    void start(Engine* engine);

    void stopAndJoin();
};
