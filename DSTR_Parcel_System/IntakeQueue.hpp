// IntakeQueue.hpp   -- TASK 1: Queue (singly linked list with head and tail)
//
// WHY A QUEUE: parcels waiting to be sorted must be handled in the order they
// arrived (first come, first served). enqueue/dequeue are both O(1).
// enqueueFront() exists only so an undone sorting step can put a parcel back at
// the front of the line instead of losing its place.
#ifndef INTAKEQUEUE_HPP
#define INTAKEQUEUE_HPP

#include <cstring>
#include "Types.hpp"

struct QueueNode {
    char       id[ID_LEN];
    QueueNode* next;
};

class IntakeQueue {
    QueueNode* head;
    QueueNode* tail;
    int        count;

    static QueueNode* makeNode(const char* id) {
        QueueNode* n = new QueueNode;
        std::strncpy(n->id, id, ID_LEN - 1);
        n->id[ID_LEN - 1] = '\0';
        n->next = nullptr;
        return n;
    }

public:
    IntakeQueue() : head(nullptr), tail(nullptr), count(0) {}
    IntakeQueue(const IntakeQueue&) = delete;
    IntakeQueue& operator=(const IntakeQueue&) = delete;
    ~IntakeQueue() { clear(); }

    void enqueue(const char* id) {                  // add at the rear
        QueueNode* n = makeNode(id);
        if (tail == nullptr) head = tail = n;
        else { tail->next = n; tail = n; }
        count++;
    }

    void enqueueFront(const char* id) {             // add at the front
        QueueNode* n = makeNode(id);
        n->next = head;
        head = n;
        if (tail == nullptr) tail = n;
        count++;
    }

    bool dequeue(char* out) {                       // remove from the front
        if (head == nullptr) return false;
        QueueNode* n = head;
        std::strcpy(out, n->id);
        head = n->next;
        if (head == nullptr) tail = nullptr;
        delete n;
        count--;
        return true;
    }

    bool peek(char* out) const {
        if (head == nullptr) return false;
        std::strcpy(out, head->id);
        return true;
    }

    bool contains(const char* id) const {
        for (QueueNode* n = head; n != nullptr; n = n->next)
            if (std::strcmp(n->id, id) == 0) return true;
        return false;
    }

    bool removeById(const char* id) {               // O(n): used when a waiting parcel is removed/undone
        QueueNode* prev = nullptr;
        for (QueueNode* n = head; n != nullptr; prev = n, n = n->next) {
            if (std::strcmp(n->id, id) == 0) {
                if (prev == nullptr) head = n->next; else prev->next = n->next;
                if (n == tail) tail = prev;
                delete n;
                count--;
                return true;
            }
        }
        return false;
    }

    void forEach(void (*visit)(const char*, int, void*), void* user) const {
        int position = 1;
        for (QueueNode* n = head; n != nullptr; n = n->next) visit(n->id, position++, user);
    }

    void clear() {
        char dummy[ID_LEN];
        while (dequeue(dummy)) {}
    }

    int  size() const    { return count; }
    bool isEmpty() const { return count == 0; }
};

#endif
