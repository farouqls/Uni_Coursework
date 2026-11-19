// CircularQueue.hpp   -- TASK 3: Circular Queue (fixed array, wrap-around indices)
//
// WHY A CIRCULAR QUEUE: counters and couriers are used again and again in rotation.
// The resource that has been idle longest sits at the front. After finishing, it
// rejoins at the rear, so the workload is shared fairly (round-robin) and no array
// space is wasted. Both operations are O(1).
// A separate 'count' variable tells FULL apart from EMPTY (front == rear in both).
#ifndef CIRCULARQUEUE_HPP
#define CIRCULARQUEUE_HPP

class CircularQueue {
    int* data;
    int  capacity;
    int  front;     // index of the first item
    int  count;     // number of items stored

public:
    explicit CircularQueue(int cap) : data(new int[cap]), capacity(cap), front(0), count(0) {}
    CircularQueue(const CircularQueue&) = delete;
    CircularQueue& operator=(const CircularQueue&) = delete;
    ~CircularQueue() { delete[] data; }

    bool enqueue(int value) {
        if (count == capacity) return false;                    // full
        data[(front + count) % capacity] = value;               // rear wraps around
        count++;
        return true;
    }

    bool dequeue(int& value) {
        if (count == 0) return false;                           // empty
        value = data[front];
        front = (front + 1) % capacity;
        count--;
        return true;
    }

    bool peek(int& value) const {
        if (count == 0) return false;
        value = data[front];
        return true;
    }

    bool contains(int value) const {
        for (int i = 0; i < count; i++)
            if (data[(front + i) % capacity] == value) return true;
        return false;
    }

    // Remove a value from the middle (resource taken out of rotation); later items shift forward.
    bool removeValue(int value) {
        for (int i = 0; i < count; i++) {
            if (data[(front + i) % capacity] == value) {
                for (int k = i; k < count - 1; k++)
                    data[(front + k) % capacity] = data[(front + k + 1) % capacity];
                count--;
                return true;
            }
        }
        return false;
    }

    int  at(int offset) const { return data[(front + offset) % capacity]; }   // offset 0 = front
    int  size() const    { return count; }
    bool isEmpty() const { return count == 0; }
    bool isFull() const  { return count == capacity; }
};

#endif
