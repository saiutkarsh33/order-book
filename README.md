## Order Book

A high-performance, low-latency order book engine implemented in C++ that achieves client, instrument and side level concurrency.

## Usage

Start the engine

./engine /tmp/orderbook.sock

Connect as a client

nc -U /tmp/orderbook.sock

## IPC

Uses Unix domain sockets to communicate between clients and the engine.

## Concurrency Overview

The engine is designed with three levels of concurrency to maximize throughput and minimize contention:

1. Client-level Concurrency

What it is: Each incoming socket connection is handled by its own thread.

Why it matters: A slow or bursty client cannot block other clients; each thread independently reads, parses, and dispatches commands.

2. Instrument-level Concurrency

What it is: Each instrument (e.g., AAPL, GOOG) has a dedicated InstrumentWorker with its own task queues and threads.

Why it matters: Orders for different instruments are processed in isolation, preventing cross-instrument contention.

3. Side-level Concurrency

What it is: Within each InstrumentWorker, there are two threads—one for buy orders and one for sell orders.

Why it matters: Buy and sell order matching for the same instrument can occur in parallel, avoiding serialization on a single thread.


