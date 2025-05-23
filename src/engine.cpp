#include <iostream> // header for standard io streams
#include <thread> // header for std:: thread

#include "io.hpp" // declares the io functions
#include "engine.hpp" // declares the functions in engine.hpp (its header file)



// Accepts a ClientConnection object.
// Spawns a new thread to handle this connection.
// Moves ownership of the connection to that new thread.
// Calls connection_thread inside that new thread.
 
void Engine::accept(ClientConnection connection) 
{
	auto thread = std::thread(&Engine::connection_thread, this, std::move(connection));

	// detach means it now can execute and run in the background
	
	thread.detach();
}

// for the below function, we return a raw ptr instead of smare unique ptr stored in the map bc
// unique ptr is move only, if u return that, the engine will no longer "own" the instrument worker anymore

InstrumentWorker* Engine::getInstrumentWorker(const std::string& instrument) {
    std::lock_guard<std::mutex> lock(workerMutex);
    auto it = instrumentWorkers.find(instrument);
    if (it == instrumentWorkers.end()) {
		// allocates the object dynamically and returned a unique ptr related to it
		// ensures that lifetime of this InstrumentWorker is managed automatically
        auto worker = std::make_unique<InstrumentWorker>(instrument);
        InstrumentWorker* workerPtr = worker.get();
        worker->start(this);  // Start its dedicated thread.
		// map now exclusively owns the worker
        instrumentWorkers[instrument] = std::move(worker);
        return workerPtr;
    }
    return it->second.get();
}

// Instead of processing orders directly, delegate them to the appropriate InstrumentWorker.
void Engine::processClientCommand(const ClientCommand& cmd) {
    std::string instr(cmd.instrument);
    if (cmd.type == input_cancel) {
        // Lookup the order in orderMap to determine its side.
        auto it = orderMap.find(cmd.order_id);
        if (it != orderMap.end()) {
            Order* order = it->second;
            // Assume your Order structure has a flag to denote sell side.
            if (order->side == Side::SELL) { // For sell orders, dispatch cancellation to sellQueue.
                InstrumentWorker* worker = getInstrumentWorker(instr);
                worker->sellQueue.push(cmd);
            } else { // For buy orders, dispatch cancellation to buyQueue.
                InstrumentWorker* worker = getInstrumentWorker(instr);
                worker->buyQueue.push(cmd);
            }
        } else {
            // If the order is not found, process cancellation immediately as rejected.
            processCancelOrder(cmd);
        }
    } else if (cmd.type == input_buy) {
        InstrumentWorker* worker = getInstrumentWorker(instr);
        worker->buyQueue.push(cmd);
    } else if (cmd.type == input_sell) {
        InstrumentWorker* worker = getInstrumentWorker(instr);
        worker->sellQueue.push(cmd);
    } else {
        SyncCerr() << "Unknown command type: " << static_cast<char>(cmd.type) << std::endl;
    }
}


void Engine::processBuyOrder(const ClientCommand& cmd) {
	// create a new Order based on the client command, better to use internal API 
	Order newOrder;
	newOrder.order_id = cmd.order_id;
	newOrder.price = cmd.price;
	newOrder.quantity = cmd.count;
	// c style string is auto converted to a std::string
	newOrder.instrument = cmd.instrument;
	if (cmd.type == input_buy) {
    newOrder.side = Side::BUY;
} else if (cmd.type == input_sell) {
    newOrder.side = Side::SELL;
}

	
	auto& sellPQ = sellOrderBooks[newOrder.instrument];

	while (!sellPQ.empty() && newOrder.quantity > 0) {

		Order topSell = sellPQ.top();
		// laze deletion
        if (topSell.cancelled) {
            sellPQ.pop();
            continue;
        }
		if (topSell.price <= newOrder.price) {
		sellPQ.pop();
		uint32_t matchedQuantity = std::min(newOrder.quantity, topSell.quantity);

		newOrder.quantity -= matchedQuantity;
		topSell.quantity -= matchedQuantity;

		intmax_t timestamp = getCurrentTimestamp();

        Output::OrderExecuted(topSell.order_id, newOrder.order_id, newOrder.order_id, 
                                topSell.price, matchedQuantity, timestamp);

		if (topSell.quantity > 0) {
			sellPQ.push(topSell);
		}
		} else {
			break;
		}
		
	}

	if (newOrder.quantity>0) {
        buyOrderBooks[newOrder.instrument].push(newOrder);
		intmax_t timestamp = getCurrentTimestamp();
		Output::OrderAdded(newOrder.order_id, newOrder.instrument.c_str(), newOrder.price, newOrder.quantity, false, timestamp);

	}

}

void Engine::processSellOrder(const ClientCommand& cmd) {
    Order newOrder;
    newOrder.order_id = cmd.order_id;
    newOrder.price = cmd.price;
    newOrder.quantity = cmd.count;
    newOrder.instrument = std::string(cmd.instrument);
    newOrder.sequence = nextSequence++;
    newOrder.cancelled = false;  


    auto& buyPQ = buyOrderBooks[newOrder.instrument];

    while (!buyPQ.empty() && newOrder.quantity > 0) {
        Order topBuy = buyPQ.top();

        if (topBuy.cancelled) {
            buyPQ.pop();
            continue;
        }
        if (topBuy.price >= newOrder.price) {
            buyPQ.pop();
            uint32_t matchedQuantity = std::min(newOrder.quantity, topBuy.quantity);
            newOrder.quantity -= matchedQuantity;
            topBuy.quantity -= matchedQuantity;
            
            intmax_t timestamp = getCurrentTimestamp();
            Output::OrderExecuted(topBuy.order_id, newOrder.order_id, newOrder.order_id, 
                                   topBuy.price, matchedQuantity, timestamp);
            if (topBuy.quantity > 0) {
                buyPQ.push(topBuy);
            }
        } else {
            break;
        }
    }
    
    if (newOrder.quantity > 0) {
        sellOrderBooks[newOrder.instrument].push(newOrder);
        intmax_t timestamp = getCurrentTimestamp();
        // For sell orders, is_sell_side is true.
        Output::OrderAdded(newOrder.order_id, newOrder.instrument.c_str(), newOrder.price, newOrder.quantity, true, timestamp);
    }
}

void Engine::processCancelOrder(const ClientCommand& cmd) {
    uint32_t orderId = cmd.order_id;
    
    bool cancelAccepted = false;
    
    auto it = orderMap.find(orderId);
    if (it != orderMap.end()) {
        Order* orderPtr = it->second;
        orderPtr->cancelled = true;
        cancelAccepted = true;
    } else {
        // Order not found – cancellation is rejected.
        cancelAccepted = false;
    }
	intmax_t timestamp = getCurrentTimestamp();
    
    // Log the cancellation event.
    Output::OrderDeleted(orderId, cancelAccepted, timestamp);
}


void Engine::connection_thread(ClientConnection connection) {
    while (true) { 
        ClientCommand cmd{};
        // Read input from the client connection.
        ReadResult result = connection.readInput(cmd);
        switch (result) {
            case ReadResult::Error:
                SyncCerr() << "Error reading input" << std::endl;
                return;  
            case ReadResult::EndOfFile:
                return; 
            case ReadResult::Success:
                break; 
        }
        processClientCommand(cmd);
    }
}

