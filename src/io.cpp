// This file contains I/O functions.


#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>

#include "io.hpp"
#include "engine.hpp"

// out of line definitions for the mutexes in SyncCerr/SyncCout
std::mutex SyncCerr::mut;
std::mutex SyncCout::mut;

// RAII

void ClientConnection::freeHandle()
{
	if(m_handle != -1)
	{   
		// closes the file descriptor to prevent respurce leaks, file struct is still there, theres a maximum number of fds an os can have
		close(m_handle);
		m_handle = -1;
	}
}

#include <unistd.h>


static ssize_t readLine(int fd, char* buf, size_t max) {
    ssize_t total = 0;
    while (total < static_cast<ssize_t>(max) - 1) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n < 0) {
            // Read error
            return -1;
        }
        if (n == 0) {
            // EOF reached
            break;
        }
        buf[total++] = c;
        if (c == '\n') {
            break;
        }
    }
    buf[total] = '\0';
    return total;
}

ReadResult ClientConnection::readInput(ClientCommand& read_into) {
    const size_t bufferSize = 256;
    char buffer[bufferSize];
    
    ssize_t n = readLine(m_handle, buffer, bufferSize);
    // If the client does ctrl D, n will be -1
    if (n < 0) {
        return ReadResult::Error;
    }
    if (n == 0) {
        return ReadResult::EndOfFile;
    }
    
    if (buffer[0] == '#' || buffer[0] == '\n') {
        return ReadResult::Success;
    }
    
    memset(&read_into, 0, sizeof(ClientCommand));
    
    char typeChar;
    if (sscanf(buffer, " %c", &typeChar) != 1) {
        return ReadResult::Error;
    }
    
    
    if (typeChar == 'B' || typeChar == 'S') {
        // %8s puts up to 8 chars for instrument (fits my char instrument[9], leaving room for null terminator)
        int ret = sscanf(buffer, " %c %u %8s %u %u", &typeChar, &read_into.order_id, read_into.instrument, &read_into.price, &read_into.count);
        if (ret != 5) {
            return ReadResult::Error;
        }
        read_into.type = static_cast<CommandType>(typeChar);
    } else if (typeChar == 'C') {
        // Format: <Type> <order_id>
        int ret = sscanf(buffer, " %c %u", &typeChar, &read_into.order_id);
        if (ret != 2) {
            return ReadResult::Error;
        }
        read_into.type = static_cast<CommandType>(typeChar);
    } else {
        return ReadResult::Error;
    }
    
    return ReadResult::Success;
}

