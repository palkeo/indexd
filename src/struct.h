#ifndef __STRUCT_H__
#define __STRUCT_H__

#include <pthread.h>
#include <set>
#include <map>
#include <vector>
#include <stdio.h> 
#include "config.h"
#include <time.h>
#include <queue>

struct Hit
{
    Hit(unsigned long p, unsigned char r) : pos(p), relevance(r) {};

    unsigned long pos;
    unsigned char relevance;

    bool operator=(const Hit& other) const {return pos == other.pos;};
    bool operator<(const Hit& other) const {return pos < other.pos;};
};

std::vector<Hit>* merge_hitlists(std::vector<Hit>* left, std::vector<Hit>* right);

struct DocAdd
{
    DocAdd() : docSize(0) {};
    unsigned long docSize;
    std::vector<Hit> hits;
};

struct DeltaStruct // For a given word, this structure contains all the changes made
{
    DeltaStruct() { if(pthread_mutex_init(&mutex, NULL) != 0) perror("DeltaStruct()"); };
    ~DeltaStruct() { if(pthread_mutex_destroy(&mutex) != 0) perror("~DeltaStruct()"); };

    pthread_mutex_t mutex; // lock when using

    std::map<DocID, DocAdd> added_docs;

    time_t last_mod;
};

struct SharedStruct
{
    SharedStruct() : force_flush(false) { pthread_mutex_init(&delta_mutex, NULL); };

    /*pthread_mutex_t opened_files_mutex; // Lock when opened_files is modified or accessed
    std::set<std::string> opened_files;*/

    std::map<std::string, DeltaStruct*> delta;
    pthread_mutex_t delta_mutex; // Lock when a new word is added or removed

    bool force_flush;
};

#endif
