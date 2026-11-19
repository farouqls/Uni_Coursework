// System.hpp
// The shared "hub": one SystemContext object holds every data structure, and every
// module receives it by reference. This is what makes the four tasks ONE integrated
// system (they read and update the same parcels, requests and resources) instead
// of four unrelated programs.
#ifndef SYSTEM_HPP
#define SYSTEM_HPP

#include "Types.hpp"
#include "DynArray.hpp"
#include "ParcelBST.hpp"
#include "IntakeQueue.hpp"
#include "RequestHeap.hpp"
#include "CircularQueue.hpp"
#include "ActivityStack.hpp"

struct SystemContext {
    // ---- Task 1 ----
    ParcelBST   registry;        // every parcel, keyed by Parcel ID   (BST)
    IntakeQueue intake;          // parcels waiting to be sorted       (Queue)

    // ---- Task 2 ----
    RequestHeap pending;                   // requests waiting for a resource (Priority Queue)
    DynArray<ServiceRequest> inProgress;   // requests a resource is working on
    DynArray<ServiceRequest> archive;      // completed and cancelled requests

    // ---- Task 3 ----
    Resource resources[MAX_RESOURCES];
    int      resourceCount;
    CircularQueue counterQueue;  // available counters in rotation    (Circular Queue)
    CircularQueue courierQueue;  // available couriers in rotation    (Circular Queue)

    // ---- Task 4 ----
    ActivityStack activities;    // newest activity on top             (Stack)

    // ---- ID / sequence generators ----
    long nextParcelNo, nextRequestNo, nextRequestSeq, nextCounterNo, nextCourierNo, nextActivitySeq;

    SystemContext()
        : resourceCount(0), counterQueue(MAX_RESOURCES), courierQueue(MAX_RESOURCES),
          nextParcelNo(1), nextRequestNo(1), nextRequestSeq(1),
          nextCounterNo(1), nextCourierNo(1), nextActivitySeq(1) {}
};

#endif
