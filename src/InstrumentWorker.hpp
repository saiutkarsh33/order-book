#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <list>
#include <map>
#include "io.hpp"
#include "ThreadSafeQueue.hpp"  // Or whatever your thread-safe queue is called

class Engine;

class InstrumentWorker {
public:
    InstrumentWorker(const std::string& instr);
    ~InstrumentWorker() { stopAndJoin(); }
    
    void start();
    void stopAndJoin();
    void addOrder(const ClientCommand& cmd);

    ThreadSafeQueue<ClientCommand> commandQueue;

private:
    enum class Side { BUY, SELL };
    struct Order {
        uint32_t    order_id;
        uint32_t    price;
        uint32_t    quantity;
        std::string instrument;
        Side        side;
    };
    using OrderPtr = std::shared_ptr<Order>;

    std::string instrument;
    std::atomic<bool> stop{false};  

    // Single worker thread per instrument
    std::thread workerThread;
    // Map order_id -> live Order* (owned inside the priority queues implicitly)
    // SHARED POINTER BECAUSE WE WANT IT TO BE OWNED BY BOTH THE BBST AND ALSO ORDER MAP;
    std::map<uint32_t, std::list<OrderPtr>, std::greater<uint32_t>> buyMap;
    std::map<uint32_t, std::list<OrderPtr>, std::less<uint32_t>> sellMap;

    struct OrderDetails {
        OrderPtr ptr;
        std::list<OrderPtr>::iterator it;
    };

    std::unordered_map<uint32_t, OrderDetails> orderMap;
    void processBuyOrder(const ClientCommand& cmd);
    void processSellOrder(const ClientCommand& cmd);
    void processCancelOrder(const ClientCommand& cmd);

};