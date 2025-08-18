#include <stdio.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "io.hpp"
#include "engine.hpp"

static int listenfd = -1;
static char* socketpath = NULL;


// Signal handler

static void handle_exit_signal(int signum)
{
    (void) signum; // tells compiler im not using this signum

	// do the exit cleanup code before terminating process immeditaley
    exit(0);
}


static void exit_cleanup(void)
{
    if (listenfd == -1)
        return;
    // on process exit, closes the listening file FD and removed the socket file from the filesystem
    close(listenfd);
    if (socketpath)
	    // removes a directory entry for a file from the filesystem
        unlink(socketpath);
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <socket path>\n", argv[0]);
        return 1;
    }

    socketpath = argv[1];
	// creare a sock stream socket (fully duplec, connection oriented byte stream)
    listenfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        perror("socket");
        return 1;
    }

    {
        struct sockaddr_un sockaddr {};
        sockaddr.sun_family = AF_UNIX;
		// Copy the provided path into sun_path (truncated if too long; you leave room for NUL).
        strncpy(sockaddr.sun_path, argv[1], sizeof(sockaddr.sun_path) - 1);
		// bind so that clients can connect
        if (bind(listenfd, (const struct sockaddr*)&sockaddr, sizeof(sockaddr)) != 0)
        {
            perror("bind");
            return 1;
        }
    }

    atexit(exit_cleanup);
	// you are trapping sigint and sigterm so they call handle_exit_signal 
    signal(SIGINT, handle_exit_signal);
    signal(SIGTERM, handle_exit_signal);

    if (listen(listenfd, 8) != 0)
    {
        perror("listen");
        return 1;
    }


    fflush(stdout);

    auto engine = new Engine();
    while (true)
    {
        fflush(stdout);

        int connfd = accept(listenfd, NULL, NULL);
        if (connfd == -1)
        {
            perror("[SERVER] accept");
            return 1;
        }

        fflush(stdout);

        engine->accept(ClientConnection(connfd));
    }

    return 0;
}
