#include "writer_thread.h"
#include <stdio.h>
#include "number_rw.h"
#include <assert.h>
#include <unistd.h> 
#include <stdlib.h>

void* writer_thread(void* args)
{
    SharedStruct* shared_struct = (SharedStruct*)args;

    while(true)
    {
        uint64_t start = time(NULL);
        std::vector<std::string> to_flush;

        // Which delta we need to flush ?
        pthread_mutex_lock(&shared_struct->delta_mutex);
        for(std::map<std::string, DeltaStruct*>::iterator it = shared_struct->delta.begin(), it_end = shared_struct->delta.end(); it != it_end; ++it)
        {
            if((start - it->second->last_mod)*60 + it->second->added_docs.size() > 10000 || shared_struct->force_flush)
                to_flush.push_back(it->first);
        }
        pthread_mutex_unlock(&shared_struct->delta_mutex);

        // Flush them !
        for(std::vector<std::string>::iterator it = to_flush.begin(), it_end = to_flush.end(); it != it_end; ++it)
        {
            pthread_mutex_lock(&shared_struct->delta_mutex);
            DeltaStruct* deltaStruct = shared_struct->delta[*it];
            shared_struct->delta.erase(*it);
            pthread_mutex_unlock(&shared_struct->delta_mutex);

            flush(*it, deltaStruct, shared_struct);
            delete deltaStruct;
        } 

        // Sleep ?
        long sleep_time = start + WRITER_LOOP_TIME - time(NULL);
        std::cout << "[Writer] sleep " << sleep_time << std::endl;
        if(sleep_time > 0)
            sleep(sleep_time);

    }
    return NULL;
}

unsigned long calculate_hitlist_size(std::vector<Hit>* hitlist)
{
    uint32_t size = 0;
    uint32_t lastPos = 0;
    for(std::vector<Hit>::iterator it=hitlist->begin(); it != hitlist->end(); it++)
    {
        size += cn_size(it->pos - lastPos + 101);
        if(it->relevance != 10)
            size += cn_size(it->relevance);
        lastPos = it->pos;
    }
    return size;
};

void flush(std::string& word, DeltaStruct* deltaStruct, SharedStruct* shared_struct)
{
    /*
     * On commence par voir si le fichier existe pas
     * On regarde quel est le plus grand ID : Si il y a moyen on append à la fin !
     * Sinon, on copie en ajoutant/modifiant à la volée l'index dans un nouveau fichier, et
     * on remplace l'ancien par le nouveau
     * Fonction énorme, complexe, beurk.
     */

    std::string index_filename = get_index_filename(word);

    QueryNode* node;
    KwNode* kw_node = new KwNode(word);
    DeltaStructNode* delta_node = new DeltaStructNode(deltaStruct, word);

    if(! delta_node->init())
    {
        std::cerr << "flush(): Unable to init DeltaNode" << std::endl;
        exit(EXIT_FAILURE);
    }
    if(! kw_node->init())
    {
        std::cerr << "flush(): Unable to init KwNode" << std::endl;
        exit(EXIT_FAILURE);
    }

    DocID lastDocID = 0;
    FILE* f;
    bool moveTmpFile = false; // Set that to true to move the TMP_FLUSH_PATH file to the index at the end of the process
    bool fullRewrite = true; // Set that to true to write a new index

    #define FOPEN_ERR(path, mode) \
        f = fopen(path, mode); \
        if(! f) { std::cerr << "flush(): unable to open the file " << path << std::endl; exit(EXIT_FAILURE); }

    if(kw_node->getDocID() == 0) // || ! kw_node->integrityCheck()) // creating a new file
    {
        node = delta_node;
        delete kw_node;
        FOPEN_ERR(index_filename.c_str(), "wb")
    }
    else if(kw_node->maxDocID() < delta_node->getDocID() && delta_node->hitCount() + kw_node->hitCount() - kw_node->indexHitCount() < INDEX_STEP * NB_INDEX_BEFORE_FULLREWRITE ) // The index file exists, but we can append our content
    {
        lastDocID = kw_node->maxDocID();
        assert(lastDocID > 0);
        node = delta_node;
        delete kw_node;
        FOPEN_ERR(index_filename.c_str(), "rb+"); // "wb" : erase the content... "a" : FORCE the fwrite() to write at the end...
        if(fseek(f, 0, SEEK_END) != 0)
        {
            std::cerr << "flush(): seek failed" << std::endl;
            exit(EXIT_FAILURE);
        }
        fullRewrite = false;
    }
    else // full rewrite
    {
        // We have to merge content and move the file
        node = new OrNode(kw_node, delta_node);

        if(! node->init())
        {
            std::cerr << "flush(): unable to init OrNode" << std::endl;
            exit(EXIT_FAILURE);
        }
        FOPEN_ERR(TMP_FLUSH_PATH, "wb");
        moveTmpFile = true;
    }

   uint32_t index_count;

    if(fullRewrite) // write a preamble
    {
        index_count = (delta_node->hitCount() + (moveTmpFile ? kw_node->hitCount() : 0)) / INDEX_STEP;
        fwrite(&index_count, sizeof(index_count), 1, f);
        DocID d = 0;
        uint64_t e = 0;
        for(uint64_t i = 0; i < index_count; ++i) // write an empty index entry
        {
            fwrite(&d, sizeof(d), 1, f);
            fwrite(&e, sizeof(e), 1, f);
        }
    }

    assert(fullRewrite != (lastDocID != 0));

    DocID docID;
    uint64_t count = 0;

#ifdef PURGE_BEFORE_DOCID
    if(fullRewrite)
        node->moveAfter(PURGE_BEFORE_DOCID);
#endif


    while(true)
    {
        node->moveAfter(lastDocID + 1);
        docID = node->getDocID();
        if(! docID)
            break;

        write_cn(f, docID - lastDocID);

        if(fullRewrite)
        {
            ++count;
            if(count % INDEX_STEP == 0)
            {
                assert(count / INDEX_STEP <= index_count);
                uint64_t pos = ftell(f);
                fseek(f, sizeof(uint32_t) + (sizeof(DocID) + sizeof(uint64_t))*(count / INDEX_STEP - 1), SEEK_SET);
                fwrite(&docID, sizeof(docID), 1, f);
                fwrite(&pos, sizeof(pos), 1, f);
                fseek(f, pos, SEEK_SET);
            }
        }

        std::vector<Hit>* hitlist = node->getHitList();
        assert(hitlist->size() > 0);
        write_cn(f, calculate_hitlist_size(hitlist) + cn_size(node->getDocSize()));
        write_cn(f, node->getDocSize());

        uint32_t lastPos = 0;
        for(std::vector<Hit>::iterator it=hitlist->begin(); it != hitlist->end(); it++)
        {
            write_cn(f, it->pos - lastPos + 101);
            unsigned char relevance = it->relevance;
            if(relevance != 10)
            {
                assert(relevance < 101);
                write_cn(f, relevance);
            }
            lastPos = it->pos;
        }
        delete hitlist;
        lastDocID = docID;
    }

    delete node;

    if(fclose(f) != 0)
    {
        std::cerr << "flush(): Unable to close file" << std::endl;
        exit(EXIT_FAILURE);
    }

    if(moveTmpFile && rename(TMP_FLUSH_PATH, index_filename.c_str()) != 0) // FIXME: LOCK ?
    {
        perror("flush()");
        exit(EXIT_FAILURE);
    }

}

