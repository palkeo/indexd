#include "config.h"
#include "struct.h"
#include "query.h"

struct ClientThreadArgs
{
    ClientThreadArgs(int s, SharedStruct* sh) : socket(s), shared_struct(sh) {};

    int socket;
    SharedStruct* shared_struct;
};

void* client_thread(void*);

QueryNode* retrieve_query_node(int socket);
void add_document(int socket, SharedStruct*);
void query_top(int socket, SharedStruct*);
void query_filter_relevance_id(int socket, SharedStruct*);
void update_docID_to_time(int socket, SharedStruct*);

#define COMMAND_QUERY_TOP 2
#define COMMAND_ADD_DOC 1
#define COMMAND_FORCE_FLUSH 3
#define COMMAND_EXIT 4
#define COMMAND_QUERY_FILTER_RELEVANCE_ID 5
#define COMMAND_QUERY_BUZZOMETRE 7

#define QNODE_AND 1
#define QNODE_OR 2
#define QNODE_KW 3
#define QNODE_EXACT 4
#define QNODE_APPROX 5
#define QNODE_NOT 6

