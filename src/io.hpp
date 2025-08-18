
#pragma once

#include <mutex>
#include <utility>
#include <cstdint>
#include <iostream>

enum CommandType
{
	input_buy = 'B',
	input_sell = 'S',
	input_cancel = 'C'
};

struct ClientCommand
{
	CommandType type;
	uint32_t order_id;
	uint32_t price;
	uint32_t count;
	char instrument[9];
};

enum class ReadResult
{
	Success,
	EndOfFile,
	Error
};

struct ClientConnection
{
	~ClientConnection() { this->freeHandle(); }
	// explicit constructor
	explicit ClientConnection(int handle) : m_handle(handle) { }
    
	// move constructor, this takes the other's m handle and makes the other's m handle -1 , which is not a valid FD
	// called when ClientConnection a(std::move(b)); or ClientConnection(std::move(b));
	// Move constructor happens when constructing a brand-new object from an rvalue.

	ClientConnection(ClientConnection&& other) : m_handle(std::exchange(other.m_handle, -1)) { }
	// overloaded =, move assignment operator
	// a = std::move(b);
	// Move assignment happens when assigning to an existing object from an rvalue.
	ClientConnection& operator=(ClientConnection&& other)
	{
		if(&other == this)
			return *this;

		this->freeHandle();
		m_handle = std::exchange(other.m_handle, -1);

		return *this;
	}
    

	// delete copy constructor, you dont want 2 connection objects to have the same file descriptor
	ClientConnection(const ClientConnection&) = delete;

	// delete the copy assignment operator 
	ClientConnection& operator=(const ClientConnection&) = delete;

	ReadResult readInput(ClientCommand& read_into);

private:
	int m_handle;
	void freeHandle();
};


// An implementation of std::osyncstream{std::cout}
// std::osyncstream would work but badly supported right now
struct SyncCout {
    static std::mutex mut;
    std::lock_guard<std::mutex> lock;

    SyncCout() : lock(mut) {}
    SyncCout(const SyncCout&) = delete;
    SyncCout& operator=(const SyncCout&) = delete;
    SyncCout(SyncCout&&) = delete;
    SyncCout& operator=(SyncCout&&) = delete;

    template <typename T>
    SyncCout& operator<<(T&& v) {
        std::cout << std::forward<T>(v);
        return *this;
    }
    SyncCout& operator<<(std::ostream& (*f)(std::ostream&)) {
        std::cout << f;
        return *this;
    }
};


// An implementation of std::osyncstream{std::cerr}
// std::osyncstream would work but badly supported right now
struct SyncCerr {
    static std::mutex mut;
    std::lock_guard<std::mutex> lock;

    SyncCerr() : lock(mut) {}
    SyncCerr(const SyncCerr&) = delete;
    SyncCerr& operator=(const SyncCerr&) = delete;
    SyncCerr(SyncCerr&&) = delete;
    SyncCerr& operator=(SyncCerr&&) = delete;

    template <typename T>
    SyncCerr& operator<<(T&& v) {
        std::cerr << std::forward<T>(v);
        return *this;
    }
    SyncCerr& operator<<(std::ostream& (*f)(std::ostream&)) {
        std::cerr << f;
        return *this;
    }
};

class Output
{
public:
	inline static void
	OrderAdded(uint32_t id, const char* symbol, uint32_t price, uint32_t count, bool is_sell_side, intmax_t output_timestamp)
	{
		SyncCout()
		    << (is_sell_side ? "S " : "B ") //
		    << id << " "                    //
		    << symbol << " "                //
		    << price << " "                 //
		    << count << " "                 //
		    << output_timestamp             //
		    << std::endl;
	}

	inline static void OrderExecuted(uint32_t resting_id,
	    uint32_t new_id,
	    uint32_t execution_id,
	    uint32_t price,
	    uint32_t count,
	    intmax_t output_timestamp)
	{
		SyncCout()
		    << "E "                //
		    << resting_id << " "   //
		    << new_id << " "       //
		    << execution_id << " " //
		    << price << " "        //
		    << count << " "        //
		    << output_timestamp    //
		    << std::endl;
	}

	inline static void OrderDeleted(uint32_t id, bool cancel_accepted, intmax_t output_timestamp)
	{
		SyncCout()
		    << "X "                            //
		    << id << " "                       //
		    << (cancel_accepted ? "A " : "R ") //
		    << output_timestamp                //
		    << std::endl;
	}
};
