#include "InstrumentWorker.hpp"
#include "engine.hpp"  
#include <iostream>

InstrumentWorker::InstrumentWorker(const std::string& instr)
    : instrument(instr) { }
    
void InstrumentWorker::start(Engine* engine) {
    // Start the buy worker thread.
    buyWorkerThread = std::thread([this, engine]() {
        try {
            while (!stop) {
                ClientCommand cmd;
                // Block until a command is available in the buy queue.
                buyQueue.wait_pop(cmd);
                // Process the command based on its type.
                if (cmd.type == input_buy) {
                    engine->processBuyOrder(cmd);
                } 
                // Unknown command types are ignored.
            }
        } catch (const std::exception& ex) {
            SyncCerr() << "Exception in buy worker for instrument " << instrument 
                       << ": " << ex.what() << std::endl;
        } catch (...) {
            SyncCerr() << "Unknown exception in buy worker for instrument " << instrument << std::endl;
        }
    });
    
    // Start the sell worker thread.
    sellWorkerThread = std::thread([this, engine]() {
        try {
            while (!stop) {
                ClientCommand cmd;
                // Block until a command is available in the sell queue.
                sellQueue.wait_pop(cmd);
                // Process the command based on its type.
                if (cmd.type == input_sell) {
                    engine->processSellOrder(cmd);
                } 
                // Unknown command types are ignored.
            }
        } catch (const std::exception& ex) {
            SyncCerr() << "Exception in sell worker for instrument " << instrument 
                       << ": " << ex.what() << std::endl;
        } catch (...) {
            SyncCerr() << "Unknown exception in sell worker for instrument " << instrument << std::endl;
        }
    });
}

    // should not just let a thread sleep forever, bc it allocates system resources like stack, thread control block etc
void InstrumentWorker::stopAndJoin() {
    stop = true;
    // Push dummy commands to unblock any waiting threads.
    buyQueue.push(ClientCommand());
    sellQueue.push(ClientCommand());
    if (buyWorkerThread.joinable())
        buyWorkerThread.join();
    if (sellWorkerThread.joinable())
        sellWorkerThread.join();
}
