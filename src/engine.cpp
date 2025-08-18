#include "engine.hpp"
#include <iostream>
#include <thread>
#include "io.hpp"

void Engine::accept(ClientConnection&& connection) {

    // Can detach here because its fire and forget, client owns its owm resource (ClientConnection , which is passed via move so no danging ref)
    // In production, u cant join() them so hard to do graceful cleanup, can cause resource leaks or zombie behavipur if misused
    // But here the client takes care of ending it (EOF or ctrl D) or error

    // engine object must outlive this thread
    std::thread(&Engine::connection_thread, this, std::move(connection)).detach();
}


InstrumentWorker& Engine::getInstrumentWorker(const std::string& instrument) {
    // Lock on the instrument worker mutex to get the actual Instrument Workers map
    std::lock_guard<std::mutex> lock(workerMutex);
    auto it = instrumentWorkers.find(instrument);
    if (it == instrumentWorkers.end()) {
        // PREV BUG: I only locked here - caused data race , if another thread inserts while this threads reads in line 18, race conditions
        // between checking the hashmap and acquiring the lock, another thread cld be iserting with the same key (or other key n rehashing) -> invaliding iterators
        // foo(std::unique_ptr<T>(new T(a)), g()); // g() might throw → leak risk pre-C++17
        auto [iterator, success] = instrumentWorkers.emplace(instrument, std::make_unique<InstrumentWorker>(instrument));
        auto& worker = *(iterator->second);
        worker.start();
        // Emplace creates the A instrument key using copy ctor and worker unique ptr using move ctor, one less memory allocation to OS if u
        // Create before and try to copy or move it in  
        // FYI: unordered_map is node-based— rehash invalidates iterators but does not invalidate pointers/references to elements. Your reference stays valid unless you erase that element.
        return worker;
    }
    return *it->second;
}

void Engine::processClientCommand(const ClientCommand& cmd) {
    std::string instr(cmd.instrument);
    auto& worker = getInstrumentWorker(instr);
    worker.addOrder(cmd);
}


void Engine::connection_thread(ClientConnection&& conn) {
    while (true) {
        ClientCommand cmd;
        ReadResult res = conn.readInput(cmd);
        if (res == ReadResult::Error || res == ReadResult::EndOfFile)
            // When return, this thread is cleaned up automatically 
            return;
        processClientCommand(cmd);
    }
}



