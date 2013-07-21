#include "struct.h"

std::vector<Hit>* merge_hitlists(std::vector<Hit>* left, std::vector<Hit>* right)
{
    std::vector<Hit>* hitList = new std::vector<Hit>();
    hitList->reserve(left->size() + right->size());
    unsigned long li = 0, ri = 0;
    unsigned long ls = left->size(), rs = right->size();
    while(li < ls && ri < rs)
    {
        if((*left)[li] < (*right)[ri])
        {
            hitList->push_back((*left)[li]);
            ++li;
        }
        else
        {
            hitList->push_back((*right)[ri]);
            ++ri;
        }
    }
    while(li < ls)
    {
        hitList->push_back((*left)[li]);
        ++li;
    }
    while(ri < rs)
    {
        hitList->push_back((*right)[ri]);
        ++ri;
    }
    return hitList;
};
