#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <iostream>
#include <stdint.h>

/*
 * A docID is an unique ID that represents a document
 */
typedef uint32_t DocID;

#define SOCKET_PATH "/tmp/indexd.sock"
#define INDEX_PATH "/DIRECTORY/" // with trailing slash
#define TMP_FLUSH_PATH "/DIRECTORY/tmp_flush"

#define WRITER_LOOP_TIME 200

#define NB_INDEX_BEFORE_FULLREWRITE 0 // number of index that can be absent, before we rewrite the file completely. 1 means that the index will always be up to date.
                                      // 0 --> will rewrite everytime. Useless, but perfect for feedspot.

#define SIZE_MALUS 2000 // the more this number is important, the less the size will be taken into account (for little size), it's like an order of magnitude of the size which will have a negative impact on ranking

#define NORM_FACTOR 200 // this factor must generally be SIZE_MALUS / default_relevance.

#define INDEX_STEP 1000 // number of hits between two index entries

#endif
