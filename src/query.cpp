#include "query.h"

#include "number_rw.h"
#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <algorithm>
#include <errno.h> 
#include <math.h>
#include <sstream>
#include <unistd.h>


/*********************************************************/
/*                     KwNode                            */  
/*********************************************************/


std::string get_index_filename(std::string word)
{
    if(word.find("/") != std::string::npos)
    {
        std::cerr << "get_index_filename(): word contains a /" << std::endl;
        exit(EXIT_FAILURE);
    }
    return INDEX_PATH + word.substr(0,1) + "/" + word;
}


KwNode::KwNode(std::string w) : word(w), file_start(NULL)
{
}

bool KwNode::init()
{
    if(file_start != NULL) // If we are already initialised, don't do nothing (useful in writer thread)
        return true;

    std::string filename = get_index_filename(word);
    std::cout << filename << std::endl;
    struct stat filestatus;
    stat(filename.c_str(), &filestatus);
    int file = open(filename.c_str(), O_RDONLY);
    if(file == -1)
    {
        docID = 0;
        index_size = 0;
        return true;
    }
    file_start = reinterpret_cast<char*>(mmap(0, filestatus.st_size, PROT_READ, MAP_PRIVATE, file, 0));
    if(close(file) != 0)
    {
        std::cerr << "KwNode::init(): Unable to close the file after mmap()" << std::endl;
        exit(EXIT_FAILURE);
    }
    if(file_start == MAP_FAILED || ! file_start)

    {
        perror("KwNode::init()");
        exit(EXIT_FAILURE);
    }
    index_size = *((uint32_t*) file_start);


    file_pos = file_start + sizeof(uint32_t) + (sizeof(DocID) + sizeof(uint64_t))*index_size;
    file_end = file_start + filestatus.st_size;
    assert(file_end - file_start > sizeof(uint32_t));

    file_index_pos = file_start + sizeof(uint32_t);
    file_index_end = file_pos;
    while(file_index_end > file_index_pos && ! *((uint64_t*) (file_index_end - sizeof(uint64_t)))) // the index can end with non-filled fields. So we will ignore them.
    {
        file_index_end -= sizeof(uint64_t) + sizeof(DocID);
        index_size--;
    }

    docID = read_cn(file_pos);
    return true;
}

void KwNode::moveAfter(DocID id)
{
    assert(file_start != NULL && file_pos < file_end && docID);

    while(file_index_pos < file_index_end && *((DocID*) file_index_pos) <= id) // index lookup
    {
        docID = *((DocID*) file_index_pos);
        file_pos = file_start + *((uint64_t*) (file_index_pos + sizeof(DocID)));
        file_index_pos += sizeof(DocID) + sizeof(uint64_t);
    }

    while(docID < id)
    {
        unsigned long size = read_cn(file_pos);
        file_pos += size;
        if(file_pos >= file_end)
        {
            assert(file_pos == file_end);
            if(file_pos != file_end)
                std::cerr << "corrupted file : " << get_index_filename(word) << std::endl;
            docID = 0;
            return;
        }
        docID += read_cn(file_pos);
    }

}

float KwNode::getRelevance() const
{
    // Cf getHitList()
    assert(file_start != NULL && file_pos < file_end);

    unsigned long relevance = 0;
    char* fpos = file_pos;
    unsigned long size = read_cn(fpos);
    char* stopAt = fpos + size;
    float docSize = read_cn(fpos) + SIZE_MALUS;
    assert(fpos < stopAt);
    while(fpos < stopAt)
    {
        pass_cn(fpos);
        if(fpos < stopAt && *(unsigned char*)fpos < 101)
            relevance += read_cn(fpos);
        else
            relevance += 10;
    }
    assert(fpos == stopAt);
    return relevance * NORM_FACTOR / docSize;
}

uint32_t KwNode::getDocSize() const
{
    assert(file_start != NULL);
    char* fpos = file_pos;
    pass_cn(fpos); // size
    return read_cn(fpos);
}

std::vector<Hit>* KwNode::getHitList() const
{
    assert(file_start != NULL);
    assert(file_pos < file_end);

    std::vector<Hit>* hitList = new std::vector<Hit>();
    uint32_t pos = 0;
    char* fpos = file_pos;
    uint32_t size = read_cn(fpos);
    char* stopAt = fpos + size;
    pass_cn(fpos); // docSize
    assert(fpos < stopAt);
    while(fpos < stopAt)
    {
        pos += read_cn(fpos) - 101;
        if(fpos < stopAt && *(unsigned char*)fpos < 101)
            hitList->push_back(Hit(pos, read_cn(fpos)));
        else
            hitList->push_back(Hit(pos, 10));
    }
    assert(fpos == stopAt);
    return hitList;
}

KwNode::~KwNode()
{
    if(file_start)
        if(munmap(file_start, file_end - file_start) != 0)
            perror("KwNode::~KwNode()");
}

uint64_t KwNode::estimateHits() const
{
    return indexHitCount();
}

bool KwNode::integrityCheck() const
{
    if(file_start == NULL)
        return true;

    uint32_t _index_size = *((uint32_t*) file_start);

    char* _file_pos = file_start + sizeof(uint32_t) + (sizeof(DocID) + sizeof(uint64_t))*_index_size;

    char* _file_index_pos = file_start + sizeof(uint32_t);
    char* _file_index_end = _file_pos;

    while(_file_index_end > _file_index_pos && ! *((uint64_t*) (_file_index_end - sizeof(uint64_t)))) // the index can end with non-filled fields. So we will ignore them.
    {
        _file_index_end -= sizeof(uint64_t) + sizeof(DocID);
        _index_size--;
    }

    DocID _docID = read_cn(_file_pos);

    
    uint32_t count = 0;

    while(true)
    {
        count++;

        if(_docID >= *((DocID*) _file_index_pos) && _file_index_pos < _file_index_end)
        {
            if(_docID != *((DocID*) _file_index_pos))
            {
                return false;
            }
            if(_file_pos != file_start + *((uint64_t*) (_file_index_pos + sizeof(DocID))))
            {
                return false;
            }
            if(count % INDEX_STEP != 0)
            {
                return false;
            }
            _file_index_pos += sizeof(DocID) + sizeof(uint64_t);
        }

        unsigned long size = read_cn(_file_pos);
        _file_pos += size;
        if(_file_pos >= file_end)
            return _file_pos == file_end && _file_index_pos == _file_index_end;
        _docID += read_cn(_file_pos);
    }


}

uint64_t KwNode::indexHitCount() const
{
    return index_size * INDEX_STEP;
}

uint64_t KwNode::hitCount() const
{
    assert(file_start != NULL);
    uint64_t count = indexHitCount();
    char* file_pos_tmp;
    if(count == 0)
    {
        file_pos_tmp = file_start + sizeof(uint32_t);
        pass_cn(file_pos_tmp);
        count = 1;
    }
    else
        file_pos_tmp = file_start + *((uint64_t*) (file_index_end - sizeof(uint64_t)));
    while(true)
    {
        unsigned long size = read_cn(file_pos_tmp);
        file_pos_tmp += size;
        if(file_pos_tmp >= file_end)
            break;
        pass_cn(file_pos_tmp);
        ++count;
    }
    assert(file_pos_tmp == file_end);
    return count;
}

DocID KwNode::maxDocID() const
{
    assert(file_start != NULL);
    DocID did = 0;
    char* file_pos_tmp;
    if(index_size == 0)
    {
        file_pos_tmp = file_start + sizeof(uint32_t);
        did += read_cn(file_pos_tmp);
    }
    else
    {
        file_pos_tmp = file_start + *((uint64_t*) (file_index_end - sizeof(uint64_t)));
        did = *((DocID*) (file_index_end - sizeof(uint64_t) - sizeof(DocID)));
    }
    while(true)
    {
        unsigned long size = read_cn(file_pos_tmp);
        file_pos_tmp += size;
        if(file_pos_tmp >= file_end)
            break;
        did += read_cn(file_pos_tmp);
    }
    assert(file_pos_tmp == file_end);
    return did;
}



/*********************************************************/
/*                     OrNode                            */  
/*********************************************************/

OrNode::OrNode(QueryNode* l, QueryNode* r) : left(l), right(r), docID(0)
{
}

OrNode::~OrNode()
{
    delete left;
    delete right;
}

bool OrNode::init()
{
    if(! (left->init() && right->init()))
        return false;

    if(left->getDocID() == 0) // if right->getDocID() == 0 : no problem
        docID = right->getDocID();
    else if(right->getDocID() == 0)
        docID = left->getDocID();
    else
        docID = std::min(left->getDocID(), right->getDocID());

    return true;
}

void OrNode::moveAfter(DocID id)
{
    // si les deux sont plus petits que l'ID voulu : on les fait avancer
    if(left->getDocID() < id && left->getDocID() != 0)
        left->moveAfter(id);
    if(right->getDocID() < id && right->getDocID() != 0)
        right->moveAfter(id);

    if(left->getDocID() == 0) // if right->getDocID() == 0 : no problem
        docID = right->getDocID();
    else if(right->getDocID() == 0)
        docID = left->getDocID();
    else
        docID = std::min(left->getDocID(), right->getDocID());
}

float OrNode::getRelevance() const
{
    if(docID == left->getDocID() && docID == right->getDocID())
        return left->getRelevance() + right->getRelevance();
    else if(docID == left->getDocID())
        return left->getRelevance();
    return right->getRelevance();
}

uint32_t OrNode::getDocSize() const
{
    if(docID == left->getDocID())
        return left->getDocSize();
    return right->getDocSize();
}

std::vector<Hit>* OrNode::getHitList() const
{
    if(docID == left->getDocID() && docID == right->getDocID())
    {
        std::vector<Hit>* left_hitList = left->getHitList();
        std::vector<Hit>* right_hitList = right->getHitList();
        std::vector<Hit>* hitList = merge_hitlists(left_hitList, right_hitList);
        delete right_hitList;
        delete left_hitList;
        return hitList;
    }
    else if(docID == left->getDocID())
        return left->getHitList();
    assert(docID == right->getDocID());
    return right->getHitList();
}

uint64_t OrNode::estimateHits() const
{
    //  We assume there will be no common hits between left and right :/
    return left->estimateHits() + right->estimateHits();
}


/*********************************************************/
/*                    AndNode                            */  
/*********************************************************/


AndNode::AndNode(QueryNode* l, QueryNode* r) : left(l), right(r), docID(0)
{
}

AndNode::~AndNode()
{
    delete left;
    delete right;
}

bool AndNode::init()
{
    if(! (left->init() && right->init()))
        return false;
    moveAfter(0);
    return true;
}

void AndNode::moveAfter(DocID id)
{
    DocID right_docID = right->getDocID(), left_docID = left->getDocID();
    if(right_docID == 0 || left_docID == 0)
    {
        docID = 0;
        return;
    }
    while(right_docID != left_docID || left_docID < id)
    {
        if(right_docID < left_docID)
        {
            right->moveAfter(std::max(id, left_docID));
            right_docID = right->getDocID();
            if(right_docID == 0)
            {
                docID = 0;
                return;
            }
        }
        else
        {
            left->moveAfter(std::max(id, right_docID));
            left_docID = left->getDocID();
            if(left_docID == 0)
            {
                docID = 0;
                return;
            }
        }
    }
    docID = left_docID;
}

float AndNode::getRelevance() const
{
    return left->getRelevance() * right->getRelevance();
}

uint32_t AndNode::getDocSize() const
{
    assert(left->getDocSize() == right->getDocSize());
    return left->getDocSize();
}

uint64_t AndNode::estimateHits() const
{
    return std::min(left->estimateHits(), right->estimateHits()) / 2;
}


/**********************************************************/
/*                 DeltaStructNode                        */  
/*********************************************************/


DeltaStructNode::DeltaStructNode(DeltaStruct* s, std::string w) : delta(s), word(w)
{
    pthread_mutex_lock(&delta->mutex);
    delta_pos = delta->added_docs.begin();
}

void DeltaStructNode::moveAfter(DocID id)
{
    while(delta_pos->first < id && delta_pos != delta->added_docs.end())
        ++delta_pos;
}

DocID DeltaStructNode::getDocID() const
{
    return (delta_pos == delta->added_docs.end()) ? 0 : delta_pos->first;
}

float DeltaStructNode::getRelevance() const
{
    assert(delta_pos != delta->added_docs.end());
    unsigned long relevance = 0;
    for(std::vector<Hit>::iterator it=delta_pos->second.hits.begin(); it != delta_pos->second.hits.end(); it++)
        relevance += it->relevance;
    return relevance * SIZE_MALUS / float(delta_pos->second.docSize + SIZE_MALUS);
}

std::vector<Hit>* DeltaStructNode::getHitList() const
{
    assert(delta_pos != delta->added_docs.end());
    std::vector<Hit>* h = new std::vector<Hit>(delta_pos->second.hits);
    return h;
}

bool DeltaStructNode::init()
{
    return true;
}

uint32_t DeltaStructNode::getDocSize() const
{
    return delta_pos->second.docSize;
}

DeltaStructNode::~DeltaStructNode()
{
    pthread_mutex_unlock(&delta->mutex);
}

uint64_t DeltaStructNode::estimateHits() const
{
    return hitCount();
}

uint64_t DeltaStructNode::hitCount() const
{
    return delta->added_docs.size();
}

/**********************************************************/
/*                      ExactNode                        */  
/********************************************************/


inline unsigned long hitlists_relevance_exact(std::vector<Hit>* hitLists[], unsigned short size)
{
    unsigned long* hitPos = new unsigned long[size];
    for(unsigned short i = 1; i < size; ++i)
        hitPos[i] = 0;

    unsigned long relevance = 0;

    for(unsigned long i = 0, size_first_hitlist = hitLists[0]->size(); i < size_first_hitlist; ++i)
    {
        unsigned short j = 1;
        unsigned int relevance_thisExpr = (*hitLists[0])[i].relevance;
        for(; j < size; ++j)
        {
            while(hitPos[j] < hitLists[j]->size() && (*hitLists[j])[hitPos[j]].pos < (*hitLists[0])[i].pos + j)
                ++hitPos[j];
            if(hitPos[j] >= hitLists[j]->size() || (*hitLists[j])[hitPos[j]].pos != (*hitLists[0])[i].pos + j)
                break;
            relevance_thisExpr += (*hitLists[j])[hitPos[j]].relevance;
        }
        if(j == size)
            relevance += relevance_thisExpr;
    }
    delete[] hitPos;
    return relevance;
}


ExactNode::ExactNode(std::vector<QueryNode*> n) : nodes(n), docID(0)
{
    assert(nodes.size() > 1);
}

ExactNode::~ExactNode()
{
    for(unsigned short i = 0, size = nodes.size(); i < size; ++i)
        delete nodes[i];
}

bool ExactNode::init()
{
    for(unsigned short i = 0, size = nodes.size(); i < size; ++i)
    {
        if(! nodes[i]->init())
            return false;
        if(nodes[i]->getDocID() == 0)
            return true;
    }

    moveAfter(0);
    return true;
}

void ExactNode::moveAfter(DocID id)
{
    unsigned short size = nodes.size();
    std::vector<Hit>** hitLists = new std::vector<Hit>*[size];
    docID = id;
    
    unsigned short i = 0;
    while(true)
    {
        nodes[i]->moveAfter(docID);
        if(nodes[i]->getDocID() != docID)
        {
            docID = nodes[i]->getDocID();
            if(docID == 0)
            {
                delete[] hitLists;
                return;
            }
            i = 0;
        }
        else if(i == size - 1) // and nodes[i]->getDocID() == docID
        {
            // docID have all the keywords we search :)

            for(unsigned short j = 0; j < size; ++j)
                hitLists[j] = nodes[j]->getHitList();

            // We calculate the relevance because anyway, we need to check if the words are making an exact expression, so we retrieve the hitlists and stuff.
            total_relevance = hitlists_relevance_exact(hitLists, size);

            for(unsigned short j = 0; j < size; ++j)
                delete hitLists[j];

            if(total_relevance > 0)
            {
                delete[] hitLists;
                return;
            }

            ++docID;
            i = 0;
        }
        else
            ++i;
    }
}

float ExactNode::getRelevance() const
{
    return total_relevance * NORM_FACTOR / float( (nodes[0]->getDocSize() + SIZE_MALUS) * nodes.size() );
}

DocID ExactNode::getDocID() const
{
    return docID;
}

uint64_t ExactNode::estimateHits() const
{
    unsigned long min = 0;
    for(unsigned short i = 0, l = nodes.size(); i < l; ++i)
    {
        unsigned long e = nodes[i]->estimateHits();
        if(e < min || min == 0)
            min = e;
    }
    return min / 5;
}

uint32_t ExactNode::getDocSize() const
{
    return nodes[0]->getDocSize();
}


/**********************************************************/
/*                      ApproxNode                        */  
/********************************************************/

inline float relevance_between_approx(unsigned long p1, unsigned long p2)
{
    // ex : apprendre python, p1=100, p2=101...
    assert(p1 != p2);

    return (p1 > p2) ? 0.3/float(p1 - p2) : 1/float(p2 - p1);
}

inline float hitlists_relevance_approx(std::vector<Hit>* hitLists[], unsigned short size)
{
    // Slow method, with an awful complexity, but for me it's quite relevant :)

    unsigned long* hitPos = new unsigned long[size];
    for(unsigned short i = 0; i < size; ++i)
        hitPos[i] = 0;

    float relevance = 0;

    while(true)
    {
        float tmp_relevance = (*hitLists[0])[hitPos[0]].relevance;
        for(unsigned short i = 1; i < size; ++i)
            tmp_relevance *= (*hitLists[i])[hitPos[i]].relevance * relevance_between_approx( (*hitLists[i-1])[hitPos[i-1]].pos, (*hitLists[i])[hitPos[i]].pos );

        relevance += tmp_relevance;

        ++hitPos[0];
        unsigned short i = 0;
        while(hitPos[i] == hitLists[i]->size())
        {
            hitPos[i] = 0;
            ++i;
            if(i == size)
            {
                delete[] hitPos;

                unsigned long sum = 0;
                for(i = 0; i < size; ++i)
                {
                    sum += hitLists[i]->size();
                    relevance /= float(hitLists[i]->size());
                }
                return relevance * sum / float(size); // relevance * average
            }
            ++hitPos[i];
        }
    }
}


ApproxNode::ApproxNode(std::vector<QueryNode*> n) : nodes(n), docID(0)
{
    assert(nodes.size() > 1);
}

ApproxNode::~ApproxNode()
{
    for(unsigned short i = 0, size = nodes.size(); i < size; ++i)
        delete nodes[i];
}

bool ApproxNode::init()
{
    for(unsigned short i = 0, size = nodes.size(); i < size; ++i)
    {
        if(! nodes[i]->init())
            return false;
        if(nodes[i]->getDocID() == 0)
            return true;
    }

    moveAfter(0);
    return true;
}

void ApproxNode::moveAfter(DocID id) // This is like an AndNode, but with an arbitrary number of subnodes
{
    unsigned short size = nodes.size();
    docID = id;

    unsigned short i = 0;
    while(true)
    {
        nodes[i]->moveAfter(docID);
        if(nodes[i]->getDocID() != docID)
        {
            docID = nodes[i]->getDocID();
            if(docID == 0)
                return;
            i = 0;
        }
        else if(i == size - 1) // and nodes[i]->getDocID() == docID
            return;
        else
            ++i;
    }
}

float ApproxNode::getRelevance() const
{
    unsigned short size = nodes.size();
    std::vector<Hit>** hitLists = new std::vector<Hit>*[size];
    for(unsigned short j = 0; j < size; ++j)
        hitLists[j] = nodes[j]->getHitList();

    float relevance = hitlists_relevance_approx(hitLists, size);

    for(unsigned short j = 0; j < size; ++j)
        delete hitLists[j];
    delete[] hitLists;
    return relevance * NORM_FACTOR / float( (nodes[0]->getDocSize() + SIZE_MALUS) * nodes.size() );
}

DocID ApproxNode::getDocID() const
{
    return docID;
}

uint64_t ApproxNode::estimateHits() const
{
    unsigned long min = 0;
    for(unsigned short i = 0, l = nodes.size(); i < l; ++i)
    {
        unsigned long e = nodes[i]->estimateHits();
        if(e < min || min == 0)
            min = e;
    }
    return min / 3;
}

uint32_t ApproxNode::getDocSize() const
{
    return nodes[0]->getDocSize();
}


/**********************************************************/
/*                      NotNode                           */  
/**********************************************************/


NotNode::NotNode(QueryNode* n) : node(n)
{
}

NotNode::~NotNode()
{
    delete node;
}

bool NotNode::init()
{
    if(! node->init())
        return false;
    moveAfter(1);
    return true;
}

void NotNode::moveAfter(DocID id)
{
    docID = id;
    if(node->getDocID() == 0)
        return;
    node->moveAfter(docID);
    while(docID == node->getDocID() && node->getDocID() != 0)
    {
        ++docID;
        node->moveAfter(docID);
    }
}

float NotNode::getRelevance() const
{
    return 1;
}

DocID NotNode::getDocID() const
{
    return docID;
}

uint64_t NotNode::estimateHits() const
{
    return 9999999999;
}

uint32_t NotNode::getDocSize() const
{
    return 0;
}
