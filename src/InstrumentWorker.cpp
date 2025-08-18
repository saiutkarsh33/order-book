#include "InstrumentWorker.hpp"
#include "engine.hpp"  
#include <iostream>

InstrumentWorker::InstrumentWorker(const std::string& instr)
    : instrument(instr) { }


void InstrumentWorker::start() {
    // Start a single worker thread per instrument
    workerThread = std::thread([this]() {
        try {
            while (!stop) {
                
                // Block until a command is available in the queue
                ClientCommand cmd = commandQueue.wait_pop();
                
                // Process the command based on its type
                switch (cmd.type) {
                    case input_buy:
                        this->processBuyOrder(cmd);
                        break;
                    case input_sell:
                        this->processSellOrder(cmd);
                        break;
                    case input_cancel:
                        this->processCancelOrder(cmd);
                        break;
                    default:
                        // Unknown command types are ignored
                        break;
                }
            }
        } catch (const std::exception& ex) {
            SyncCerr() << "Exception in worker for instrument " << instrument 
                       << ": " << ex.what() << std::endl;
        } catch (...) {
            SyncCerr() << "Unknown exception in worker for instrument " << instrument << std::endl;
        }
    });
}


void InstrumentWorker::processBuyOrder(const ClientCommand& cmd) {
    auto orderPtr = std::make_shared<Order>();
    orderPtr->order_id   = cmd.order_id;
    orderPtr->price      = cmd.price;
    orderPtr->quantity   = cmd.count;
    orderPtr->instrument = cmd.instrument;
    orderPtr->side       = Side::BUY;

    // Reason why i made pq take in ptrs is bc intially above I was doing raw value then in the below,
    // I was adding a reference, once the Order gets popped off the stack, Im left w a dangling reference

    // Made OrderPtr a shared pointer because I want it to be copied onto the pq -> so it exists in pq and one in the map
    // map one goes out of scope when cancelled, heap goes ouf of scope when popped
    // so automatic destructor and dealloacation of memory

    while (!sellMap.empty() && orderPtr->quantity > 0 && sellMap.begin()->first <= orderPtr->price) {
        auto lowestPriceLevelIterator = sellMap.begin();
        auto& list = lowestPriceLevelIterator->second;
        while (orderPtr->quantity && !list.empty()) {
            auto top = list.front();
            uint32_t m = std::min(orderPtr->quantity, top->quantity);
            orderPtr->quantity -= m;
            top->quantity      -= m;
            auto ts = getCurrentTimestamp();
            Output::OrderExecuted(top->order_id, orderPtr->order_id,
                                    orderPtr->order_id, top->price, m, ts);
            if (top->quantity == 0) {
                orderMap.erase(top->order_id);
                list.pop_front();
            }
        }
        if (list.empty()) {
            sellMap.erase(lowestPriceLevelIterator);
        }
    }
    if (orderPtr->quantity > 0) {
        auto& priceList = buyMap[orderPtr->price];
        priceList.push_back(orderPtr);
        auto it = std::prev(priceList.end());

        orderMap[orderPtr->order_id] = OrderDetails {
            orderPtr, it
        };
        auto ts = getCurrentTimestamp();
        Output::OrderAdded(orderPtr->order_id, orderPtr->instrument.c_str(),
                            orderPtr->price, orderPtr->quantity, false, ts);
    }
}



void InstrumentWorker::processSellOrder(const ClientCommand& cmd) {
    auto orderPtr = std::make_shared<Order>();
    orderPtr->order_id   = cmd.order_id;
    orderPtr->price      = cmd.price;
    orderPtr->quantity   = cmd.count;
    orderPtr->instrument = cmd.instrument;
    orderPtr->side       = Side::SELL;

    // Cross against best bids
    while (!buyMap.empty() && orderPtr->quantity > 0 && buyMap.begin()->first >= orderPtr->price) {
        auto highestPriceLevelIterator = buyMap.begin();
        auto& list = highestPriceLevelIterator->second;   // std::list<OrderPtr>&

        while (orderPtr->quantity && !list.empty()) {
            auto top = list.front();
            uint32_t m = std::min(orderPtr->quantity, top->quantity);
            orderPtr->quantity -= m;
            top->quantity      -= m;

            auto ts = getCurrentTimestamp();
            Output::OrderExecuted(top->order_id, orderPtr->order_id,
                                  orderPtr->order_id, top->price, m, ts);

            if (top->quantity == 0) {
                orderMap.erase(top->order_id);
                list.pop_front();
            }
        }
        if (list.empty()) {
            buyMap.erase(highestPriceLevelIterator);
        }
    }

    // Rest remainder on the ask side
    if (orderPtr->quantity > 0) {
        auto& priceList = sellMap[orderPtr->price]; // creates the level if missing
        priceList.push_back(orderPtr);
        auto it = std::prev(priceList.end());

        orderMap[orderPtr->order_id] = OrderDetails{
            orderPtr, it
        };

        auto ts = getCurrentTimestamp();
        Output::OrderAdded(orderPtr->order_id, orderPtr->instrument.c_str(),
                           orderPtr->price, orderPtr->quantity, /*isSell=*/true, ts);
    }
}


void InstrumentWorker::processCancelOrder(const ClientCommand& cmd) {
    const uint32_t id = cmd.order_id;
    bool ok = false;
    auto iterator = orderMap.find(id);
    if (iterator != orderMap.end()) {
            auto& orderPtr = iterator->second.ptr;

            if (orderPtr->side == Side::BUY) {
            
                auto lvl = buyMap.find(orderPtr->price);
                if (lvl != buyMap.end())
                {
                    lvl->second.erase(iterator->second.it);      // O(1) erase by iterator
                    if (lvl->second.empty())
                        buyMap.erase(lvl);          // drop empty price level
                    ok = true;
                }
            }
            else {
                auto lvl = sellMap.find(orderPtr->price);
                if (lvl != sellMap.end())
                {
                    lvl->second.erase(iterator->second.it);      
                    if (lvl->second.empty())
                        sellMap.erase(lvl);        
                    ok = true;
                }                
            }

            // Always remove from the index to avoid dangling iterators / double-cancels
            orderMap.erase(iterator);
        }
    
    
    Output::OrderDeleted(id, ok, getCurrentTimestamp());
}


void InstrumentWorker::stopAndJoin() {
    stop = true;
    // Push dummy command to unblock if thread is waiting
    commandQueue.push(ClientCommand());
    if (workerThread.joinable())
        // join bc threads use up space -> they have their own portion of stack memory, register values, kernel stack space (used during syscalls and interrupts)
        // in Linux , each process and thread both are represented by a task_struct, threads just share certain resources such as the virtual address space
        // In in a classic OS textbook, thread's info lives in the PCB (or a TCB linked to it).
        workerThread.join();
}

void InstrumentWorker::addOrder(const ClientCommand& cmd) {
    commandQueue.push(cmd);
}