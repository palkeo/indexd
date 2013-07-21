#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "client_thread.h"

void* server_thread(void* args)
{
    int s;
    if((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
        exit(EXIT_FAILURE);
    struct sockaddr_un local;
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, SOCKET_PATH);
    unlink(local.sun_path); // if the socket already exists, remove it
    if(bind(s, (struct sockaddr *)&local, strlen(local.sun_path) + sizeof(local.sun_family)) != 0)
    {
        perror("server_thread()->bind()");
        exit(EXIT_FAILURE);
    }
    if(listen(s, 10) != 0) // 10 is the number of connections that can be queued before calling accept
    {
        perror("server_thread()->listen()");
        exit(EXIT_FAILURE);
    }

    SharedStruct* shared_struct = (SharedStruct*) args;

    // Wait for connections and create a thread for each of them.

    pthread_attr_t clients_attr;
    pthread_attr_init(&clients_attr);
    pthread_attr_setdetachstate(&clients_attr, PTHREAD_CREATE_DETACHED);

    while(true)
    {
        struct sockaddr_un remote;
        int s_remote;
        socklen_t remote_len = sizeof(remote);
        if ((s_remote = accept(s, (struct sockaddr *)&remote, &remote_len)) < 0)
        {
            perror("server_thread()->accept()");
            exit(EXIT_FAILURE);
        }

        pthread_t client;
        ClientThreadArgs* args = new ClientThreadArgs(s_remote, shared_struct);
        if(pthread_create(&client, &clients_attr, client_thread, args) != 0)
        {
            perror("server_thread()->pthread_create(client)");
            delete args;
            exit(EXIT_FAILURE);
        }
    }
};
