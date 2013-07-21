#ifndef __QUERY_H__
#define __QUERY_H__

#include <assert.h>
#include "config.h"
#include "struct.h"
#include <map>

std::string get_index_filename(std::string);

class QueryNode /* I'm an abstract class */
{
public:
    virtual ~QueryNode() {};

    // Init the node, return true if the init is a success
    virtual bool init() = 0;

    // Move to the doc specified by the id, or after if not found
    virtual void moveAfter(DocID id) = 0;

    // Get the relevance for the actual doc
    virtual float getRelevance() const = 0;

    // Get the actual docID
    virtual DocID getDocID() const = 0;

    // Get the list of hits for the actual docID
    // WARNING : For performance reason (no useless copy), this function ALLOCATE the hitlist, so it needs to be DELETED !
    virtual std::vector<Hit>* getHitList() const {assert(false);};

    // Get the size of the doc (total number of hits, for all words) : the number of words in the document
    virtual uint32_t getDocSize() const = 0;

    // Try to estimate the number of hits, bigger the return value is, bigger the number of hits may be.
    virtual uint64_t estimateHits() const = 0;

    bool operator<(const QueryNode& other) const { return estimateHits() < other.estimateHits(); };


};

class KwNode : public QueryNode
{
public:
    KwNode(std::string);
    ~KwNode();

    bool init();
    void moveAfter(DocID id);
    std::vector<Hit>* getHitList() const;
    float getRelevance() const;
    DocID getDocID() const { return docID; };
    uint32_t getDocSize() const;
    uint64_t estimateHits() const;
    uint64_t indexHitCount() const;
    uint64_t hitCount() const;

    bool integrityCheck() const;

    DocID maxDocID() const;

protected:
    char* file_start;
    char* file_end;
    char* file_pos;
    char* file_index_pos;
    char* file_index_end;
    uint32_t index_size;
    std::string word;
    DocID docID;
};


class OrNode : public QueryNode
{
public:
    OrNode(QueryNode* l, QueryNode* r);
    ~OrNode();
    bool init();
    DocID getDocID() const { return docID; };
    std::vector<Hit>* getHitList() const;
    uint32_t getDocSize() const;
    uint64_t estimateHits() const;
    float getRelevance() const;
    void moveAfter(DocID);

protected:
    DocID docID;
    QueryNode* left;
    QueryNode* right;
};


class AndNode : public QueryNode
{
public:
    AndNode(QueryNode* l, QueryNode* r);
    ~AndNode();
    bool init();
    DocID getDocID() const { return docID; };

    uint32_t getDocSize() const;
    uint64_t estimateHits() const;
    float getRelevance() const;
    void moveAfter(DocID);

protected:
    DocID docID;
    QueryNode* left;
    QueryNode* right;
};


class DeltaStructNode : public QueryNode
{
public:
    DeltaStructNode(DeltaStruct*, std::string);
    ~DeltaStructNode();

    bool init();

    void moveAfter(DocID id);
    std::vector<Hit>* getHitList() const;
    float getRelevance() const;
    DocID getDocID() const;
    uint32_t getDocSize() const;
    uint64_t estimateHits() const;
    uint64_t hitCount() const;

protected:
    DeltaStruct* delta;
    std::map<DocID, DocAdd>::iterator delta_pos;
    std::string word;
};

class ExactNode : public QueryNode
{
public:
    ExactNode(std::vector<QueryNode*>);
    ~ExactNode();
    bool init();
    void moveAfter(DocID id);
    float getRelevance() const;
    DocID getDocID() const;
    uint64_t estimateHits() const;
    uint32_t getDocSize() const;

protected:
    unsigned long total_relevance;
    std::vector<QueryNode*> nodes;
    DocID docID;
};

class ApproxNode : public QueryNode
{
public:
    ApproxNode(std::vector<QueryNode*>);
    ~ApproxNode();
    bool init();
    void moveAfter(DocID id);
    float getRelevance() const;
    DocID getDocID() const;
    uint64_t estimateHits() const;
    uint32_t getDocSize() const;

protected:
    std::vector<QueryNode*> nodes;
    DocID docID;
};

class NotNode : public QueryNode
{
public:
    NotNode(QueryNode*);
    ~NotNode();
    bool init();
    void moveAfter(DocID id);
    float getRelevance() const;
    DocID getDocID() const;
    uint64_t estimateHits() const;
    uint32_t getDocSize() const;

protected:
    QueryNode* node;
    DocID docID;
};

#endif
