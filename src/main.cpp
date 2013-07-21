#include <string.h>

#include <signal.h>

#include <pthread.h>
#include <stdlib.h>

#include "config.h"
#include "server_thread.h"
#include "writer_thread.h"
#include "struct.h"

int main(int argc, char** argv)
{
    SharedStruct shared_struct;

    signal(SIGPIPE, SIG_IGN);

    // start the server thread

    pthread_t server;
    if(pthread_create(&server, NULL, server_thread, &shared_struct) != 0)
    {
        perror("main()->pthread_create(server)");
        return EXIT_FAILURE;
    }

    // start the writer thread

    pthread_t writer;
    if(pthread_create(&writer, NULL, writer_thread, &shared_struct) != 0)
    {
        perror("main()->pthread_create(writer)");
        return EXIT_FAILURE;
    }


    // INIT DONE

    // Main loop, reserved for future use.
    // For example : Starting/stopping fetcher threads, monitoring cpu/memory taken by the daemon...
    pthread_join(server, NULL); // infinite sleep

    return EXIT_SUCCESS;
};
