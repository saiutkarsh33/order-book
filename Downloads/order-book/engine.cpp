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



void Engine::connection_thread(ClientConnection connection)
{
	while(true) // only terminates if error or end of file
	{
		ClientCommand input {};
		switch(connection.readInput(input)) // returns a ReadResult
		{
			case ReadResult::Error: SyncCerr {} << "Error reading input" << std::endl;
			case ReadResult::EndOfFile: return;
			case ReadResult::Success: break; // process the next command
		}

		// Functions for printing output actions in the prescribed format are
		// provided in the Output class:
		switch(input.type)
		{
			case input_cancel: {
				SyncCerr {} << "Got cancel: ID: " << input.order_id << std::endl; // thread safe logging

				// Remember to take timestamp at the appropriate time, or compute
				// an appropriate timestamp!
				auto output_time = getCurrentTimestamp();
				Output::OrderDeleted(input.order_id, true, output_time);
				break;
			}

			default: {
				SyncCerr {}
				    << "Got order: " << static_cast<char>(input.type) << " " << input.instrument << " x " << input.count << " @ "
				    << input.price << " ID: " << input.order_id << std::endl;

				// Remember to take timestamp at the appropriate time, or compute
				// an appropriate timestamp!
				auto output_time = getCurrentTimestamp();
				Output::OrderAdded(input.order_id, input.instrument, input.price, input.count, input.type == input_sell,
				    output_time);
				break;
			}
		}

		// Additionally:

		// Remember to take timestamp at the appropriate time, or compute
		// an appropriate timestamp!
		intmax_t output_time = getCurrentTimestamp();

		// Check the parameter names in `io.hpp`.

		// TODO : THIS IS HARDCODED!
		Output::OrderExecuted(123, 124, 1, 2000, 10, output_time);
	}
}
