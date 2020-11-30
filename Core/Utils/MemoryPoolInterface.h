#ifndef MEMORYPOOLINTERFACE_H
#define MEMORYPOOLINTERFACE_H

#include "MemoryPool.h"

#define MEM_POOL_MAX_ELEMENTS 1000

template <typename T>
class MemoryPoolInterface
{
    public:
        MemoryPoolInterface();
        virtual ~MemoryPoolInterface();

        static MemoryPool<T> mPool;
        static void *operator new(std::size_t size);
        static void operator delete(void *p);
    protected:
        typename MemoryPool<T>::MemoryPoolElement *m_thisElement;
    private:
};

template <typename T>
MemoryPool<T> MemoryPoolInterface<T>::mPool(MEM_POOL_MAX_ELEMENTS);

template <typename T>
void *MemoryPoolInterface<T>::operator new (std::size_t size) {
    typename MemoryPool<T>::MemoryPoolElement *thisElement = mPool.getFreeElement();

    if (thisElement == NULL) {
        return NULL;
    }

    thisElement->element->m_thisElement = thisElement;
    return (void *) thisElement->element;
}

template <typename T>
void MemoryPoolInterface<T>::operator delete (void *p) {
    typename MemoryPool<T>::MemoryPoolElement *el = ((T *)(p))->m_thisElement;
    mPool.freeElementFromPool(el);
}

template <typename T>
MemoryPoolInterface<T>::MemoryPoolInterface() {

}

template <typename T>
MemoryPoolInterface<T>::~MemoryPoolInterface() {

}

#endif // MEMORYPOOLINTERFACE_H
