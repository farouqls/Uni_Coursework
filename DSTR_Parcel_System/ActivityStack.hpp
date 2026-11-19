// ActivityStack.hpp   -- TASK 4: Stack (dynamic array, push at the top)
//
// WHY A STACK: staff need the MOST RECENT activity first (last in, first out) and
// undo always works backwards from the newest. push is O(1) amortised, peek is O(1).
// History is never deleted: reversing an activity marks it 'reversed' and pushes a
// new Reversal entry, so the audit trail stays complete.
// at()/fromTop() are read-only views used to display the history.
#ifndef ACTIVITYSTACK_HPP
#define ACTIVITYSTACK_HPP

#include "Types.hpp"

class ActivityStack {
    Activity* data;
    int       count;
    int       capacity;

    void grow() {
        int newCap = capacity * 2;
        Activity* bigger = new Activity[newCap];
        for (int i = 0; i < count; i++) bigger[i] = data[i];
        delete[] data;
        data = bigger;
        capacity = newCap;
    }

public:
    ActivityStack() : data(new Activity[32]), count(0), capacity(32) {}
    ActivityStack(const ActivityStack&) = delete;
    ActivityStack& operator=(const ActivityStack&) = delete;
    ~ActivityStack() { delete[] data; }

    void push(const Activity& a) {
        if (count == capacity) grow();
        data[count++] = a;
    }

    bool peek(Activity& out) const {
        if (count == 0) return false;
        out = data[count - 1];
        return true;
    }

    int  size() const    { return count; }
    bool isEmpty() const { return count == 0; }

    Activity&       at(int bottomIndex)       { return data[bottomIndex]; }   // 0 = oldest
    const Activity& at(int bottomIndex) const { return data[bottomIndex]; }
    Activity&       fromTop(int i)            { return data[count - 1 - i]; } // 0 = newest
};

#endif
