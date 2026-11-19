// DynArray.hpp
// Small self-made growable array (we may not use std::vector). Used for lists of
// in-progress and archived service requests.
#ifndef DYNARRAY_HPP
#define DYNARRAY_HPP

template <typename T>
class DynArray {
    T*  items;
    int count;
    int capacity;

    void grow() {                                   // double when full
        int newCap = capacity * 2;
        T* bigger = new T[newCap];
        for (int i = 0; i < count; i++) bigger[i] = items[i];
        delete[] items;
        items = bigger;
        capacity = newCap;
    }

public:
    DynArray() : items(new T[8]), count(0), capacity(8) {}
    DynArray(const DynArray&) = delete;
    DynArray& operator=(const DynArray&) = delete;
    ~DynArray() { delete[] items; }

    void add(const T& value) {
        if (count == capacity) grow();
        items[count++] = value;
    }
    void removeAt(int index) {                      // shift left to close the gap: O(n)
        for (int i = index; i < count - 1; i++) items[i] = items[i + 1];
        count--;
    }
    int  size() const { return count; }
    T&       at(int i)       { return items[i]; }
    const T& at(int i) const { return items[i]; }
};

#endif
