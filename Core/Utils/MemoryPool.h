#ifndef MEMORYPOOL_H
#define MEMORYPOOL_H

#include <stdlib.h>
#include <iostream>

template <typename T>
class MemoryPool
{
    public:
        struct MemoryPoolElement {
            MemoryPoolElement *next, *prev;
            T *element;

            MemoryPoolElement();
            ~MemoryPoolElement();
        };

        MemoryPool(unsigned int numberOfElements = 100);
        virtual ~MemoryPool();
        void freeElementFromPool(MemoryPool<T>::MemoryPoolElement *element);
        MemoryPoolElement *getFreeElement();
    protected:
    private:
        unsigned int numberOfElements;

        MemoryPoolElement head;
        MemoryPoolElement *freeElement;
        MemoryPoolElement *currentTail;
        T *elements;

        void addElementToList(MemoryPoolElement *newElement);
};

template <typename T>
MemoryPool<T>::MemoryPool(unsigned int numberOfElements) : numberOfElements (numberOfElements),
                                                           freeElement (NULL),
                                                           currentTail (NULL) {
    elements = new T[numberOfElements];

    for (unsigned int i = 0; i < numberOfElements; i++) {
        MemoryPoolElement *newElement = new MemoryPoolElement();
        newElement->element = &elements[i];

        addElementToList (newElement);
    }
}

template <typename T>
void MemoryPool<T>::addElementToList (MemoryPoolElement *newElement) {
    if (currentTail ==  NULL) {
        head.next = newElement;
        newElement->prev = &head;
        currentTail = newElement;
        freeElement = newElement;
    } else {
        currentTail->next = newElement;
        newElement->prev = currentTail;
        currentTail = newElement;
    }
}

template <typename T>
typename MemoryPool<T>::MemoryPoolElement *MemoryPool<T>::getFreeElement() {
    MemoryPoolElement *ret = freeElement;
    if (freeElement != NULL) {
        freeElement = freeElement->next;
        return ret;
    } else {
        return NULL;
    }
}

template <typename T>
void MemoryPool<T>::freeElementFromPool(MemoryPool<T>::MemoryPoolElement *element) {
    MemoryPoolElement *prev = element->prev;
    MemoryPoolElement *next = element->next;

    prev->next = next;
    next->prev = prev;

    currentTail->next = element;
    element->prev = currentTail;
    element->next = NULL;

    currentTail = element;
    if (freeElement == NULL) {
        freeElement = element;
    }
}

template <typename T>
MemoryPool<T>::MemoryPoolElement::MemoryPoolElement() : next (NULL), prev (NULL), element (NULL) {

}

template <typename T>
MemoryPool<T>::MemoryPoolElement::~MemoryPoolElement() {
    next = NULL;
    prev = NULL;
    element = NULL;
}

template <typename T>
MemoryPool<T>::~MemoryPool() {
    MemoryPoolElement *itr = head.next;

    while (itr != NULL) {
        MemoryPoolElement *next = itr->next;
        delete itr;
        itr = next;
    }
    delete[] elements;
}


#endif // MEMORYPOOL_H
