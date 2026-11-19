// ResourceModule.cpp  -- TASK 3: Collection Counter and Delivery Resource Management
//
// Data structure used here:
//   * Circular Queue (ctx.counterQueue, ctx.courierQueue): the AVAILABLE resources of
//     each kind, in rotation. Front = idle longest = next to be assigned (round-robin).
//     A finished resource rejoins at the rear. The queues hold indexes into ctx.resources[].
#include <iostream>
#include <cstdio>
#include <cstring>
#include "ResourceModule.hpp"
#include "RequestModule.hpp"
#include "ActivityModule.hpp"
#include "Utils.hpp"

using namespace std;

static CircularQueue& queueFor(SystemContext& ctx, int resType) {
    return resType == RT_COUNTER ? ctx.counterQueue : ctx.courierQueue;
}

static int findResourceIndex(SystemContext& ctx, const char* id) {
    for (int i = 0; i < ctx.resourceCount; i++)
        if (strcmp(ctx.resources[i].id, id) == 0) return i;
    return -1;
}

bool addResource(SystemContext& ctx, int type, const char* name, bool logIt) {
    if (ctx.resourceCount >= MAX_RESOURCES) return false;
    if (name[0] == '\0') return false;
    Resource r = Resource();
    if (type == RT_COUNTER) snprintf(r.id, sizeof(r.id), "CTR%03ld", ctx.nextCounterNo++);
    else                    snprintf(r.id, sizeof(r.id), "DEL%03ld", ctx.nextCourierNo++);
    copyStr(r.name, sizeof(r.name), name);
    r.type = type;
    r.status = RES_AVAILABLE;
    ctx.resources[ctx.resourceCount] = r;
    queueFor(ctx, type).enqueue(ctx.resourceCount);      // joins the rear of the rotation
    ctx.resourceCount++;
    if (logIt) {
        char desc[DESC_LEN];
        snprintf(desc, sizeof(desc), "Registered %s %s (%s)", resourceTypeName(type), r.id, r.name);
        logActivity(ctx, AT_RESOURCE_CHANGED, "", "", r.id, -1, -1, desc, false);
    }
    return true;
}

bool resourceTypeAvailable(SystemContext& ctx, int resType) {
    return !queueFor(ctx, resType).isEmpty();
}

bool assignResource(SystemContext& ctx, int resType, const char* parcelId, const char* requestId,
                    const char* specificResId, char* outResId) {
    // duplicate active assignment: this request must not already hold a resource
    for (int i = 0; i < ctx.resourceCount; i++) {
        if (strcmp(ctx.resources[i].assignedRequestId, requestId) == 0) {
            cout << "  Error: request " << requestId << " already has an active assignment ("
                 << ctx.resources[i].id << ").\n";
            return false;
        }
    }
    CircularQueue& q = queueFor(ctx, resType);
    int idx = -1;
    if (specificResId != nullptr && specificResId[0] != '\0') {
        idx = findResourceIndex(ctx, specificResId);
        if (idx < 0) { cout << "  Error: invalid Resource ID '" << specificResId << "'.\n"; return false; }
        const Resource& r = ctx.resources[idx];
        if (r.type != resType) {
            cout << "  Error: " << r.id << " is a " << resourceTypeName(r.type) << "; this service needs a "
                 << resourceTypeName(resType) << ".\n";
            return false;
        }
        if (r.status != RES_AVAILABLE) {
            cout << "  Error: " << r.id << " is not available (" << resourceStatusName(r.status) << ").\n";
            return false;
        }
        q.removeValue(idx);                              // taken out of the middle of the rotation
    } else {
        if (!q.dequeue(idx)) {                           // empty queue = nobody free
            cout << "  No " << resourceTypeName(resType) << " is available right now (all occupied or unavailable).\n";
            return false;
        }
    }
    Resource& r = ctx.resources[idx];
    r.status = RES_OCCUPIED;
    copyStr(r.assignedParcelId, sizeof(r.assignedParcelId), parcelId);
    copyStr(r.assignedRequestId, sizeof(r.assignedRequestId), requestId);
    copyStr(outResId, ID_LEN, r.id);
    return true;
}

bool releaseResource(SystemContext& ctx, const char* resourceId) {
    int idx = findResourceIndex(ctx, resourceId);
    if (idx < 0) { cout << "  Error: invalid Resource ID '" << resourceId << "'.\n"; return false; }
    Resource& r = ctx.resources[idx];
    if (r.assignedRequestId[0] == '\0') {
        cout << "  Error: resource " << r.id << " has nothing assigned, so it cannot be released.\n";
        return false;
    }
    r.assignedParcelId[0] = '\0';
    r.assignedRequestId[0] = '\0';
    if (r.status == RES_UNAVAILABLE) return true;        // went off duty while busy: stays out of rotation
    r.status = RES_AVAILABLE;
    queueFor(ctx, r.type).enqueue(idx);                  // rejoins the REAR of the rotation
    return true;
}

static void printQueueOrder(SystemContext& ctx, int resType) {
    CircularQueue& q = queueFor(ctx, resType);
    cout << "  " << resourceTypeName(resType) << " rotation (next first): ";
    if (q.isEmpty()) { cout << "(empty - none free)\n"; return; }
    for (int i = 0; i < q.size(); i++) {
        cout << ctx.resources[q.at(i)].id;
        if (i < q.size() - 1) cout << " -> ";
    }
    cout << "\n";
}

void displayResources(SystemContext& ctx) {
    printHeading("Collection Counters and Delivery Resources");
    if (ctx.resourceCount == 0) { cout << "  No resources registered.\n"; return; }
    printPad("ID", 8); printPad("Type", 20); printPad("Name", 16); printPad("Status", 12);
    printPad("Parcel", 8); printPad("Request", 8);
    cout << "\n";
    printRule(76);
    int count[RES_STATUS_COUNT] = { 0, 0, 0 };
    for (int i = 0; i < ctx.resourceCount; i++) {
        const Resource& r = ctx.resources[i];
        printPad(r.id, 8); printPad(resourceTypeName(r.type), 20); printPad(r.name, 16);
        printPad(resourceStatusName(r.status), 12);
        printPad(r.assignedParcelId[0] ? r.assignedParcelId : "-", 8);
        printPad(r.assignedRequestId[0] ? r.assignedRequestId : "-", 8);
        cout << "\n";
        count[r.status]++;
    }
    cout << "\n  Available: " << count[RES_AVAILABLE] << "   Occupied: " << count[RES_OCCUPIED]
         << "   Unavailable: " << count[RES_UNAVAILABLE] << "\n";
    printQueueOrder(ctx, RT_COUNTER);
    printQueueOrder(ctx, RT_COURIER);
}

// ============================ menu actions ============================
static void registerResourceMenu(SystemContext& ctx) {
    printHeading("Register a Counter or Delivery Resource");
    if (ctx.resourceCount >= MAX_RESOURCES) { cout << "  The resource table is full (" << MAX_RESOURCES << ").\n"; return; }
    cout << "  1) Collection counter   2) Delivery courier\n";
    int type = readInt("  Choose (1-2): ", 1, 2) - 1;
    char name[NAME_LEN];
    readLine("  Name / label: ", name, sizeof(name));
    if (!addResource(ctx, type, name, true)) { cout << "\n  Registration failed: a name is required.\n"; return; }
    cout << "\n  Registered " << ctx.resources[ctx.resourceCount - 1].id << " and added to the rotation.\n";
}

static void assignMenu(SystemContext& ctx) {
    printHeading("Assign a Resource to a Pending Service Request");
    displayPendingRequests(ctx);
    if (ctx.pending.isEmpty()) return;
    char reqId[ID_LEN], resId[ID_LEN];
    readLine("\n  Request ID to start (blank = cancel): ", reqId, sizeof(reqId));
    normalizeId(reqId);
    if (reqId[0] == '\0') return;
    readLine("  Resource ID (blank = next in rotation): ", resId, sizeof(resId));
    normalizeId(resId);
    startRequest(ctx, reqId, resId);
}

static void updateStatusMenu(SystemContext& ctx) {
    printHeading("Update a Resource's Status");
    displayResources(ctx);
    if (ctx.resourceCount == 0) return;
    char id[ID_LEN];
    readLine("\n  Resource ID: ", id, sizeof(id));
    normalizeId(id);
    int idx = findResourceIndex(ctx, id);
    if (idx < 0) { cout << "\n  Error: invalid Resource ID '" << id << "'.\n"; return; }
    Resource& r = ctx.resources[idx];
    cout << "  " << r.id << " is currently " << resourceStatusName(r.status) << ".\n";
    cout << "  1) Mark UNAVAILABLE (off duty / maintenance)   2) Mark AVAILABLE (back in service)\n";
    int choice = readInt("  Choose (1-2): ", 1, 2);
    char desc[DESC_LEN];

    if (choice == 1) {
        if (r.status == RES_UNAVAILABLE) { cout << "\n  Already unavailable.\n"; return; }
        if (r.status == RES_AVAILABLE) queueFor(ctx, r.type).removeValue(idx);   // out of the rotation
        bool busy = (r.status == RES_OCCUPIED);
        r.status = RES_UNAVAILABLE;
        snprintf(desc, sizeof(desc), "%s marked unavailable", r.id);
        logActivity(ctx, AT_RESOURCE_CHANGED, "", "", r.id, -1, -1, desc, false);
        cout << "\n  " << r.id << " is now unavailable.";
        if (busy) cout << " It will finish request " << r.assignedRequestId << " and then stay out of rotation.";
        cout << "\n";
    } else {
        if (r.status == RES_AVAILABLE) { cout << "\n  Already available.\n"; return; }
        if (r.status == RES_OCCUPIED)  { cout << "\n  Refused: " << r.id << " is busy with request " << r.assignedRequestId
                                              << ". It becomes available when that request is completed or cancelled.\n"; return; }
        if (r.assignedRequestId[0] != '\0') {            // was marked off duty while still working
            r.status = RES_OCCUPIED;
            cout << "\n  " << r.id << " is still working on " << r.assignedRequestId << ", so it is Occupied again.\n";
        } else {
            r.status = RES_AVAILABLE;
            queueFor(ctx, r.type).enqueue(idx);
            cout << "\n  " << r.id << " is available again and rejoined the rotation.\n";
        }
        snprintf(desc, sizeof(desc), "%s marked available", r.id);
        logActivity(ctx, AT_RESOURCE_CHANGED, "", "", r.id, -1, -1, desc, false);
    }
}

void resourceMenu(SystemContext& ctx) {
    while (true) {
        printHeading("TASK 3 - Collection Counter and Delivery Resource Management");
        cout << "  1. Register a new counter / courier\n"
             << "  2. Display resource availability and rotation order\n"
             << "  3. Assign a resource to a pending request\n"
             << "  4. Update a resource's status (available / unavailable)\n"
             << "  0. Back to main menu\n";
        switch (readInt("Choose (0-4): ", 0, 4)) {
            case 1: registerResourceMenu(ctx); break;
            case 2: displayResources(ctx);     break;
            case 3: assignMenu(ctx);           break;
            case 4: updateStatusMenu(ctx);     break;
            case 0: return;
        }
    }
}
