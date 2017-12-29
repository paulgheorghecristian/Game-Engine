#ifndef MEMORYPOOLTEST_H
#define MEMORYPOOLTEST_H

#include <iostream>
#include "MemoryPoolInterface.h"

class MemoryPoolTest : public MemoryPoolInterface <MemoryPoolTest>
{
    public:
        MemoryPoolTest();
        virtual ~MemoryPoolTest();
        int x;
};
#endif // MEMORYPOOLTEST_H
