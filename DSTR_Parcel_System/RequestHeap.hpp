// RequestHeap.hpp   -- TASK 2: Priority Queue (binary min-heap in an array)
//
// WHY A PRIORITY QUEUE: "which request is handled next" depends on urgency, not
// just arrival order. The RULE (justify this in the Q&A):
//     1. lower urgency number first (1 = Urgent, 2 = Normal, 3 = Low)
//     2. if urgency is equal, the earlier request (smaller sequence number) first
// insert and extract are O(log n); looking at the top is O(1).
#ifndef REQUESTHEAP_HPP
#define REQUESTHEAP_HPP

#include <cstring>
#include "Types.hpp"

class RequestHeap {
    ServiceRequest* data;
    int count;
    int capacity;

    // true when a should be served before b
    static bool higher(const ServiceRequest& a, const ServiceRequest& b) {
        if (a.urgency != b.urgency) return a.urgency < b.urgency;
        return a.sequence < b.sequence;
    }

    void swapItems(int i, int j) { ServiceRequest t = data[i]; data[i] = data[j]; data[j] = t; }

    void siftUp(int i) {
        while (i > 0) {
            int parent = (i - 1) / 2;
            if (higher(data[i], data[parent])) { swapItems(i, parent); i = parent; }
            else break;
        }
    }

    void siftDown(int i) {
        while (true) {
            int left = 2 * i + 1, right = 2 * i + 2, best = i;
            if (left < count  && higher(data[left],  data[best])) best = left;
            if (right < count && higher(data[right], data[best])) best = right;
            if (best == i) break;
            swapItems(i, best);
            i = best;
        }
    }

    void grow() {
        int newCap = capacity * 2;
        ServiceRequest* bigger = new ServiceRequest[newCap];
        for (int i = 0; i < count; i++) bigger[i] = data[i];
        delete[] data;
        data = bigger;
        capacity = newCap;
    }

public:
    RequestHeap() : data(new ServiceRequest[16]), count(0), capacity(16) {}

    // Deep copy: lets us list requests in priority order without disturbing the real heap.
    RequestHeap(const RequestHeap& other)
        : data(new ServiceRequest[other.capacity]), count(other.count), capacity(other.capacity) {
        for (int i = 0; i < count; i++) data[i] = other.data[i];
    }
    RequestHeap& operator=(const RequestHeap&) = delete;
    ~RequestHeap() { delete[] data; }

    void insert(const ServiceRequest& r) {
        if (count == capacity) grow();
        data[count] = r;
        siftUp(count);
        count++;
    }

    bool peek(ServiceRequest& out) const {
        if (count == 0) return false;
        out = data[0];
        return true;
    }

    bool extract(ServiceRequest& out) {
        if (count == 0) return false;
        out = data[0];
        data[0] = data[count - 1];
        count--;
        if (count > 0) siftDown(0);
        return true;
    }

    int indexOf(const char* id) const {             // O(n): the heap is only ordered by priority
        for (int i = 0; i < count; i++)
            if (std::strcmp(data[i].id, id) == 0) return i;
        return -1;
    }

    bool getById(const char* id, ServiceRequest& out) const {
        int i = indexOf(id);
        if (i < 0) return false;
        out = data[i];
        return true;
    }

    // Remove any request (used for cancel/undo): swap with the last, then restore the heap.
    bool removeById(const char* id, ServiceRequest* removed = nullptr) {
        int i = indexOf(id);
        if (i < 0) return false;
        if (removed != nullptr) *removed = data[i];
        data[i] = data[count - 1];
        count--;
        if (i < count) { siftUp(i); siftDown(i); }
        return true;
    }

    const ServiceRequest& at(int i) const { return data[i]; }   // array order, not priority order
    int  size() const    { return count; }
    bool isEmpty() const { return count == 0; }
};

#endif
