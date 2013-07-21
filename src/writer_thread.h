#include "config.h"
#include "struct.h"
#include "query.h"

void* writer_thread(void*);

void flush(std::string&, DeltaStruct*, SharedStruct*);
