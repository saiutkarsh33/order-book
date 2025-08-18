#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <queue>

#include "io.hpp"
#include "InstrumentWorker.hpp"

class Engine {
public:
    // Accept incoming client connection
    void accept(ClientConnection&& conn);

    // Entry point for a parsed ClientCommand
    void processClientCommand(const ClientCommand& cmd);

    // Lookup (or create) the worker for this instrument
    InstrumentWorker& getInstrumentWorker(const std::string& instrument);

private:
    void connection_thread(ClientConnection&& conn);

    // Per-instrument workers
    // Can be pointer bc im not gonna delete the InstrumentWorker in my program 
    std::unordered_map<std::string, std::unique_ptr<InstrumentWorker>> instrumentWorkers;
    std::mutex workerMutex;

};


inline std::chrono::microseconds::rep getCurrentTimestamp() 

{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

