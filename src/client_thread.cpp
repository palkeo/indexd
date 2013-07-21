#include "client_thread.h"
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

void* client_thread(void* args)
{
    int socket = ((ClientThreadArgs*) args)->socket;
    SharedStruct* shared_struct = ((ClientThreadArgs*) args)->shared_struct;
    delete (ClientThreadArgs*) args;

    while(true)
    {
        unsigned char command;
        int n = read(socket, &command, sizeof(command));
        if(n < 1)
            break;
        assert(n == sizeof(command));
        
        switch(command)
        {
            case COMMAND_QUERY_TOP:
                query_top(socket, shared_struct);
            break;
            case COMMAND_ADD_DOC:
                add_document(socket, shared_struct);
            break;
            case COMMAND_FORCE_FLUSH:
                shared_struct->force_flush = true;
            break;
            case COMMAND_EXIT:
                exit(EXIT_FAILURE);
            break;
            case COMMAND_QUERY_FILTER_RELEVANCE_ID:
                query_filter_relevance_id(socket, shared_struct);
            break;
            default:
                std::cerr << "unknown command" << std::endl;
                goto end_client_thread;
        }
        
    }

    end_client_thread:

    if(close(socket) != 0)
        std::cerr << "Error: unable to close the socket at the end of client_thread" << std::endl;
    return NULL;
};

QueryNode* retrieve_query_node(int socket)
{

    unsigned char type;
    read(socket, &type, sizeof(type));

    #define RETRIEVE_NODELIST(nodes) \
        unsigned short numberOfNodes; \
        read(socket, &numberOfNodes, sizeof(numberOfNodes)); \
        std::vector<QueryNode*> nodes; \
        nodes.resize(numberOfNodes); \
        for(unsigned short i = 0; i < numberOfNodes; ++i) \
        { \
            nodes[i] = retrieve_query_node(socket); \
            if(! nodes[i]) { \
                for(unsigned short j = 0; j <= i; j++) delete nodes[i]; \
                return NULL; } \
        }


    switch(type)
    {
        case QNODE_AND:
            return new AndNode(retrieve_query_node(socket), retrieve_query_node(socket));
        case QNODE_OR:
            return new OrNode(retrieve_query_node(socket), retrieve_query_node(socket));
        case QNODE_KW:
        {
            unsigned char word_len;
            read(socket, &word_len, sizeof(word_len));
            char buffer[255];
            read(socket, &buffer, word_len);
            return new KwNode(std::string(buffer, word_len));
        }
        case QNODE_EXACT:
        {
            RETRIEVE_NODELIST(nodes)
            return new ExactNode(nodes);
        }
        case QNODE_APPROX:
        {
            RETRIEVE_NODELIST(nodes)
            return new ApproxNode(nodes);
        }
        case QNODE_NOT:
            return new NotNode(retrieve_query_node(socket));
        
        default:
            std::cerr << "unknown node type" << std::endl;
            return NULL;
    };
};

void add_document(int socket, SharedStruct* shared_struct)
{
    DocID docID;
    read(socket, &docID, sizeof(docID));

    uint32_t word_number;
    read(socket, &word_number, sizeof(word_number));

    uint32_t weight_sum;
    read(socket, &weight_sum, sizeof(weight_sum));

    assert(docID > 0 && word_number > 0 && weight_sum > word_number);

    std::string word;
    for(uint32_t i = 0; i < word_number; ++i)
    {
        uint8_t word_len;
        read(socket, &word_len, sizeof(word_len));
        assert(word_len < 50);
        char buffer[255];
        read(socket, &buffer, word_len);
        std::string word = std::string(buffer, word_len);

        uint8_t weight = 10;
        read(socket, &weight, sizeof(weight));
        assert(weight < 101);

        pthread_mutex_lock(&shared_struct->delta_mutex);

        DeltaStruct* deltaStruct = shared_struct->delta[word];
        if(deltaStruct == 0)
            deltaStruct = shared_struct->delta[word] = new DeltaStruct();

        pthread_mutex_unlock(&shared_struct->delta_mutex);
        pthread_mutex_lock(&deltaStruct->mutex);

        time(&deltaStruct->last_mod);

        DocAdd* d = &deltaStruct->added_docs[docID];

        if(d->docSize == 0) // if it's a new DocAdd
            d->docSize = weight_sum;
        assert(d->docSize == weight_sum);

        assert(d->hits.rbegin() != d->hits.rend() ? d->hits.rbegin()->pos < i : 1);
        d->hits.push_back(Hit(i, weight));

        pthread_mutex_unlock(&deltaStruct->mutex);
    }
};

//////////////////////////////////////////////////////////////////////////    QUERY ALGORITHMS    //////////////////////////////////////////////////////////


void query_top(int socket, SharedStruct* shared_struct)
{
    // This function return the bests results matching a query. Starting by the result number [start]. It returns [size] results.
    // It's for "normal" search purposes.

    QueryNode* query = retrieve_query_node(socket);

    uint32_t start, size, stop;
    DocID docID_start, docID_stop, docID;

    // Read data, and convert timestamp to docID if needed
    {
        uint64_t time_start, time_stop;

        read(socket, &docID_start, sizeof(DocID));
        read(socket, &docID_stop, sizeof(DocID));

        read(socket, &start, sizeof(start));
        read(socket, &size, sizeof(size));

    }

    stop = start + size;
    
    if((! query) || (! query->init()))
    {
        std::cerr << "query_top(): unable to init query" << std::endl;
        docID = 0;
        write(socket, &docID, sizeof(docID)); // count
        write(socket, &docID, sizeof(docID));
        delete query;
        return;
    }

    std::multimap<float, DocID> results;

    if(docID_start && query->getDocID())
        query->moveAfter(docID_start);

    docID = query->getDocID();

    float minRelevance = 0;
    uint32_t count = 0;

    while(docID && ((! docID_stop) || docID <= docID_stop))
    {
        count++;
        float relevance = query->getRelevance();

        if(relevance > minRelevance)
        {
            results.insert(std::pair<float, DocID>(relevance, docID));
            if(results.size() > stop)
            {
                results.erase(results.begin());
                minRelevance = results.begin()->first;
            }
        }

        query->moveAfter(docID + 1);
        docID = query->getDocID();
    }

    write(socket, &count, sizeof(count));

    // Send the results
    {
        std::multimap<float, DocID>::reverse_iterator it = results.rbegin(), it_end = results.rend();

        for(unsigned long i = 0; i < start && it != it_end; ++it, ++i); // Skip some

        for(; it != it_end; ++it)
            write(socket, &it->second, sizeof(it->second));
    }

    docID = 0;
    write(socket, &docID, sizeof(docID)); // it will send 0 to indicate the end of the list

    delete query;
};

void query_filter_relevance_id(int socket, SharedStruct* shared_struct)
{
    // This function return ALL the results that have a relevance greater (or equal) than [min_relevance], AND a docID greater than [docID].
    // Initially developed for Feedspot.

    QueryNode* query = retrieve_query_node(socket);

    float min_relevance;
    DocID docID_start, docID_stop, docID;

    // Read data, and convert timestamp to docID if needed
    {
        uint64_t time_start, time_stop;

        read(socket, &min_relevance, sizeof(min_relevance));
        read(socket, &docID_start, sizeof(DocID));
        read(socket, &docID_stop, sizeof(DocID));

    }


    if((! query) || (! query->init()) || min_relevance < 0)
    {
        delete query;
        std::cerr << "query_filter_relevance_id(): unable to init query, or min_relevance < 0" << std::endl;
        docID = 0;
        write(socket, &docID, sizeof(docID));
        return;
    }

    if(docID_start && query->getDocID())
        query->moveAfter(docID_start);

    docID = query->getDocID();

    while(docID && ((! docID_stop) || docID <= docID_stop))
    {
        if(min_relevance == 0 || query->getRelevance() >= min_relevance)
            write(socket, &docID, sizeof(docID));

        query->moveAfter(docID + 1);
        docID = query->getDocID();
    }

    docID = 0;
    write(socket, &docID, sizeof(docID)); // it will send 0 to indicate the end of the list
    delete query;
};
