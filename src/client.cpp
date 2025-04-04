#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <poll.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/un.h>
#include <sys/socket.h>

#include <atomic>

#include "io.hpp"

// Macros are replaced by the preprocessor before compilation, dont take upm memory or require run time intialisation unlike global variables

#define INPUT_CANCEL_ORDER 'C'
#define INPUT_BUY_ORDER 'B'
#define INPUT_SELL_ORDER 'S'

static char* line_buffer;
static size_t line_buffer_size = 0;
static std::atomic<bool> main_is_exiting = 0;

// purpose of poll thread is to continously check whether the server (matching engine) has closed or error has occurred

static void* poll_thread(void* fdptr)
{
	// in unix/linux, files can be actual files or sockets or pipes or terminals, any I/O resource
	// A socket file descriptor is simply an integer that refers to a communication endpoint

	struct pollfd pfd {};
	pfd.fd = (int) (long) fdptr;
	pfd.events = 0;

	while(!main_is_exiting)
	{
		if(poll(&pfd, 1, -1) == -1)
		{
			perror("poll");
			_exit(1);
		}
		if(main_is_exiting)
		{
			break;
		}
		if(pfd.revents & (POLLERR | POLLHUP))
		{
			fprintf(stderr, "Connection closed by server\n");
			_exit(0);
		}
	}
	return 0;
}

int main(int argc, char* argv[])
{
	if(argc < 2)
	{
		fprintf(stderr, "Usage: %s <path of socket to connect to> < <input>\n", argv[0]);
		return 1;
	}

    // unix domain socket, used for inter process communication on the same host

	// AF_UNIX domain (address family) uses a filesystem to identify the socket endpoint

	// sock stream indicated a stream based socket, analogous to a TCP like connection

	int clientfd = socket(AF_UNIX, SOCK_STREAM, 0); // returns an integer file descriptor, kernel maintains a table mapping file descriptor integer to the actual ds representring the open file
	if(clientfd == -1)
	{
		perror("socket");
		return 1;
	}
    
	{
		struct sockaddr_un sockaddr {};
		sockaddr.sun_family = AF_UNIX;
		// copy the command line arguments into the file path
		strncpy(sockaddr.sun_path, argv[1], sizeof(sockaddr.sun_path) - 1);
		//attempt to connect the clientfd to the domain addess described by socketaddr
		// for a pipe, usually one process creates a pipe n then forks child processes that inherit the pipe file descriptors 
		// unix domain socket is typically used for a client server model, where one process (the server) calls socket(), listen n accept from multiple clients
		if(connect(clientfd, (const struct sockaddr*) &sockaddr, sizeof(sockaddr)) != 0)
		{
			perror("connect");
			return 1;
		}
	}

	// wraps the integer file descriptor into a FILE* stream
	// allows me to use C's higher level I/O functions like fwrite, fread instead of lower level send/recv or read/write

	FILE* client = fdopen(clientfd, "r+");
	// disable buffeiring / batch processing
	
	setbuf(client, NULL);

	pthread_t poll_thread_handle;
	if(pthread_create(&poll_thread_handle, NULL, poll_thread, (void*) (long) clientfd) < 0)
	{
		fprintf(stderr, "Failed to create poll thread\n");
		return 1;
	}

	while(1)
	{
		ClientCommand input {};

		ssize_t line_length = getline(&line_buffer, &line_buffer_size, stdin);
		if(line_length == -1)
			break;

		switch(line_buffer[0])
		{
			case '#':
			case '\n': continue;
			case INPUT_CANCEL_ORDER:
				input.type = input_cancel;
				if(sscanf(line_buffer + 1, " %u", &input.order_id) != 1)
				{
					fprintf(stderr, "Invalid cancel order: %s\n", line_buffer);
					return 1;
				}
				break;
			case INPUT_BUY_ORDER: input.type = input_buy; goto new_order;
			case INPUT_SELL_ORDER:
				input.type = input_sell;
			new_order:
				if(sscanf(line_buffer + 1, " %u %8s %u %u", &input.order_id, input.instrument, &input.price, &input.count) != 4)
				{
					fprintf(stderr, "Invalid new order: %s\n", line_buffer);
					return 1;
				}
				break;
			default: fprintf(stderr, "Invalid command '%c'\n", line_buffer[0]); return 1;
		}

		if(fwrite(&input, 1, sizeof(input), client) != sizeof(input))
		{
			fprintf(stderr, "Failed to write command\n");
			return 1;
		}
	}

	main_is_exiting = 1;
	fclose(client);

	return ferror(stderr) ? 1 : 0;
}
