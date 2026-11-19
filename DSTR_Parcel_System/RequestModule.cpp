// RequestModule.cpp  -- TASK 2: Parcel Service and Collection Management
//
// Data structure used here:
//   * Priority Queue (ctx.pending): requests waiting for a resource, best first.
//     Rule: lower urgency number first; equal urgency -> earlier request first.
//   Requests then move  pending -> in progress -> archive (completed / cancelled).
#include <iostream>
#include <cstdio>
#include <cstring>
#include "RequestModule.hpp"
#include "IntakeModule.hpp"
#include "ResourceModule.hpp"
#include "ActivityModule.hpp"
#include "Utils.hpp"

using namespace std;

static int findInProgress(SystemContext& ctx, const char* id) {
    for (int i = 0; i < ctx.inProgress.size(); i++)
        if (strcmp(ctx.inProgress.at(i).id, id) == 0) return i;
    return -1;
}
static int findArchived(SystemContext& ctx, const char* id) {
    for (int i = 0; i < ctx.archive.size(); i++)
        if (strcmp(ctx.archive.at(i).id, id) == 0) return i;
    return -1;
}
static bool requestIdExists(SystemContext& ctx, const char* id) {
    return ctx.pending.indexOf(id) >= 0 || findInProgress(ctx, id) >= 0 || findArchived(ctx, id) >= 0;
}

bool parcelHasActiveRequest(SystemContext& ctx, const char* parcelId) {
    for (int i = 0; i < ctx.pending.size(); i++)
        if (strcmp(ctx.pending.at(i).parcelId, parcelId) == 0) return true;
    for (int i = 0; i < ctx.inProgress.size(); i++)
        if (strcmp(ctx.inProgress.at(i).parcelId, parcelId) == 0) return true;
    return false;
}

// ============================================================
//  Core operations (no menus, so the menus and the demo share them)
// ============================================================
bool createRequestRecord(SystemContext& ctx, const char* parcelId, int serviceType, int urgency,
                         const char* note, char* err, size_t errSize) {
    Parcel* p = ctx.registry.find(parcelId);
    if (p == nullptr) { snprintf(err, errSize, "Invalid Parcel ID '%s'.", parcelId); return false; }
    if (isTerminalStatus(p->status)) {
        snprintf(err, errSize, "Parcel %s is already %s.", parcelId, parcelStatusName(p->status));
        return false;
    }
    if (!canRequestService(p->status)) {
        snprintf(err, errSize, "Parcel %s is '%s'; it must be sorted for a next action before a service can be requested.",
                 parcelId, parcelStatusName(p->status));
        return false;
    }
    if (!serviceAllowedForStatus(serviceType, p->status)) {
        snprintf(err, errSize, "'%s' is not allowed while the parcel is '%s'.",
                 serviceTypeName(serviceType), parcelStatusName(p->status));
        return false;
    }
    if (parcelHasActiveRequest(ctx, parcelId)) {
        snprintf(err, errSize, "Parcel %s already has an active service request (duplicate request).", parcelId);
        return false;
    }

    ServiceRequest r = ServiceRequest();
    do { snprintf(r.id, sizeof(r.id), "REQ%03ld", ctx.nextRequestNo++); } while (requestIdExists(ctx, r.id));
    copyStr(r.parcelId, sizeof(r.parcelId), parcelId);
    r.serviceType = serviceType;
    r.urgency = urgency;
    r.sequence = ctx.nextRequestSeq++;
    getTimestamp(r.created, sizeof(r.created));
    r.status = RS_PENDING;
    r.parcelStatusBefore = p->status;
    copyStr(r.note, sizeof(r.note), note);
    ctx.pending.insert(r);

    char desc[DESC_LEN];
    snprintf(desc, sizeof(desc), "Created %s request %s (%s) for parcel %s",
             urgencyName(urgency), r.id, serviceTypeName(serviceType), parcelId);
    logActivity(ctx, AT_REQUEST_CREATED, parcelId, r.id, "", -1, -1, desc, true);
    return true;
}

static void archiveCancelled(SystemContext& ctx, ServiceRequest& r, const char* reason) {
    r.status = RS_CANCELLED;
    copyStr(r.note, sizeof(r.note), reason);
    ctx.archive.add(r);
}

bool startRequest(SystemContext& ctx, const char* reqId, const char* resId) {
    ServiceRequest req = ServiceRequest();

    if (reqId == nullptr || reqId[0] == '\0') {
        if (ctx.pending.isEmpty()) { cout << "  There are no pending service requests to process.\n"; return false; }
        // Walk the requests in priority order and take the first whose resource type is free,
        // so one busy pool (e.g. all counters) never stalls requests that need the other pool.
        RequestHeap order(ctx.pending);
        ServiceRequest candidate;
        int skipped = 0;
        bool found = false;
        while (order.extract(candidate)) {
            if (resourceTypeAvailable(ctx, serviceResourceType(candidate.serviceType))) { req = candidate; found = true; break; }
            skipped++;
        }
        if (!found) {
            cout << "  Pending requests exist, but no suitable counter/courier is free right now.\n";
            return false;
        }
        if (skipped > 0)
            cout << "  (Skipped " << skipped << " higher-priority request(s) still waiting for a busy resource type.)\n";
    } else {
        if (!ctx.pending.getById(reqId, req)) {
            if (findInProgress(ctx, reqId) >= 0)      cout << "  Request " << reqId << " is already in progress.\n";
            else if (findArchived(ctx, reqId) >= 0)   cout << "  Request " << reqId << " is already completed or cancelled and cannot be processed again.\n";
            else                                      cout << "  Error: invalid Request ID '" << reqId << "'.\n";
            return false;
        }
    }

    Parcel* p = ctx.registry.find(req.parcelId);
    if (p == nullptr || !serviceAllowedForStatus(req.serviceType, p->status)) {
        cout << "  Request " << req.id << " can no longer be processed ("
             << (p == nullptr ? "its parcel record no longer exists" : "the parcel is now '")
             << (p == nullptr ? "" : parcelStatusName(p->status)) << (p == nullptr ? "" : "'")
             << "). It has been cancelled.\n";
        ServiceRequest removed;
        ctx.pending.removeById(req.id, &removed);
        archiveCancelled(ctx, removed, "Auto-cancelled: parcel no longer eligible");
        char desc[DESC_LEN];
        snprintf(desc, sizeof(desc), "Request %s auto-cancelled (parcel no longer eligible)", req.id);
        logActivity(ctx, AT_REQUEST_CANCELLED, req.parcelId, req.id, "", -1, -1, desc, false);
        return false;
    }

    char assigned[ID_LEN];
    if (!assignResource(ctx, serviceResourceType(req.serviceType), req.parcelId, req.id, resId, assigned))
        return false;                                    // request stays pending

    ServiceRequest started;
    ctx.pending.removeById(req.id, &started);
    int before = p->status;
    started.status = RS_IN_PROGRESS;
    started.parcelStatusBefore = before;
    copyStr(started.resourceId, sizeof(started.resourceId), assigned);
    p->status = PS_IN_SERVICE;                           // allowed: every pending state -> In Service
    ctx.inProgress.add(started);

    char desc[DESC_LEN];
    snprintf(desc, sizeof(desc), "Started %s: %s assigned to parcel %s (%s)",
             started.id, assigned, started.parcelId, serviceTypeName(started.serviceType));
    logActivity(ctx, AT_REQUEST_STARTED, started.parcelId, started.id, assigned, before, PS_IN_SERVICE, desc, true);
    cout << "  Request " << started.id << " started: " << assigned << " assigned, parcel "
         << started.parcelId << " is now In Service.\n";
    return true;
}

bool completeRequestRecord(SystemContext& ctx, const char* reqId, char* err, size_t errSize) {
    int i = findInProgress(ctx, reqId);
    if (i < 0) {
        if (ctx.pending.indexOf(reqId) >= 0)       snprintf(err, errSize, "Request %s is still pending; start it before completing it.", reqId);
        else if (findArchived(ctx, reqId) >= 0)    snprintf(err, errSize, "Request %s is already completed or cancelled.", reqId);
        else                                       snprintf(err, errSize, "Invalid Request ID '%s'.", reqId);
        return false;
    }
    ServiceRequest req = ctx.inProgress.at(i);
    Parcel* p = ctx.registry.find(req.parcelId);
    int outcome = serviceOutcomeStatus(req.serviceType);
    if (p == nullptr) { snprintf(err, errSize, "The parcel record for %s no longer exists.", reqId); return false; }
    if (!isValidTransition(p->status, outcome)) {
        snprintf(err, errSize, "Cannot complete: parcel is '%s', so it cannot become '%s'.",
                 parcelStatusName(p->status), parcelStatusName(outcome));
        return false;
    }
    if (!releaseResource(ctx, req.resourceId)) { snprintf(err, errSize, "Could not release resource %s.", req.resourceId); return false; }

    int before = p->status;
    p->status = outcome;
    req.status = RS_COMPLETED;
    snprintf(req.note, sizeof(req.note), "Completed: parcel %s", parcelStatusName(outcome));
    ctx.inProgress.removeAt(i);
    ctx.archive.add(req);

    char desc[DESC_LEN];
    snprintf(desc, sizeof(desc), "Completed %s: parcel %s is now %s; %s released",
             req.id, req.parcelId, parcelStatusName(outcome), req.resourceId);
    logActivity(ctx, AT_REQUEST_COMPLETED, req.parcelId, req.id, req.resourceId, before, outcome, desc, false);
    return true;
}

bool cancelRequestRecord(SystemContext& ctx, const char* reqId, char* err, size_t errSize) {
    ServiceRequest req;
    char desc[DESC_LEN];

    if (ctx.pending.getById(reqId, req)) {               // case 1: still waiting - just remove it
        ctx.pending.removeById(reqId, &req);
        archiveCancelled(ctx, req, "Cancelled while pending");
        snprintf(desc, sizeof(desc), "Cancelled pending request %s for parcel %s", req.id, req.parcelId);
        logActivity(ctx, AT_REQUEST_CANCELLED, req.parcelId, req.id, "", -1, -1, desc, false);
        return true;
    }
    int i = findInProgress(ctx, reqId);
    if (i >= 0) {                                        // case 2: being worked on - undo its effects
        req = ctx.inProgress.at(i);
        Parcel* p = ctx.registry.find(req.parcelId);
        if (p == nullptr || p->status != PS_IN_SERVICE) {
            snprintf(err, errSize, "Parcel state is inconsistent; cannot cancel %s.", reqId);
            return false;
        }
        if (!releaseResource(ctx, req.resourceId)) { snprintf(err, errSize, "Could not release resource %s.", req.resourceId); return false; }
        int before = p->status;
        p->status = req.parcelStatusBefore;              // back to the state it was in before service
        ctx.inProgress.removeAt(i);
        archiveCancelled(ctx, req, "Cancelled while in progress");
        snprintf(desc, sizeof(desc), "Cancelled in-progress %s; parcel %s restored to %s; %s released",
                 req.id, req.parcelId, parcelStatusName(req.parcelStatusBefore), req.resourceId);
        logActivity(ctx, AT_REQUEST_CANCELLED, req.parcelId, req.id, req.resourceId, before, p->status, desc, false);
        return true;
    }
    if (findArchived(ctx, reqId) >= 0) snprintf(err, errSize, "Request %s is already completed or cancelled.", reqId);
    else                               snprintf(err, errSize, "Invalid Request ID '%s'.", reqId);
    return false;
}

// ============================================================
//  Displays
// ============================================================
void displayPendingRequests(SystemContext& ctx) {
    printHeading("Pending Requests (highest priority first)");
    if (ctx.pending.isEmpty()) { cout << "  No requests are waiting.\n"; return; }
    RequestHeap order(ctx.pending);                      // copy, so the real heap is untouched
    ServiceRequest r;
    printPad("No.", 4);
    printRequestHeader();
    int rank = 1;
    while (order.extract(r)) {
        char num[12];
        snprintf(num, sizeof(num), "%d", rank++);
        printPad(num, 4);
        printRequestRow(r);
    }
}

static void displayInProgress(SystemContext& ctx) {
    cout << "\n  In progress:\n";
    if (ctx.inProgress.size() == 0) { cout << "  (none)\n"; return; }
    printRequestHeader();
    for (int i = 0; i < ctx.inProgress.size(); i++) printRequestRow(ctx.inProgress.at(i));
}

// ============================================================
//  Menu actions
// ============================================================
static void createRequestMenu(SystemContext& ctx) {
    printHeading("Create a Service Request");
    char parcelId[ID_LEN];
    readLine("  Parcel ID: ", parcelId, sizeof(parcelId));
    normalizeId(parcelId);
    Parcel* p = ctx.registry.find(parcelId);
    if (p == nullptr) { cout << "\n  Error: invalid Parcel ID '" << parcelId << "'.\n"; return; }
    cout << "\n";
    printParcelDetails(*p);

    int allowed[SVC_COUNT], n = 0;
    cout << "\n  Services available for this parcel:\n";
    for (int s = 0; s < SVC_COUNT; s++) {
        if (serviceAllowedForStatus(s, p->status) && canRequestService(p->status)) {
            allowed[n++] = s;
            cout << "   " << n << ") " << serviceTypeName(s) << "\n";
        }
    }
    if (n == 0) {
        char err[200];
        createRequestRecord(ctx, parcelId, SVC_COLLECTION, URG_NORMAL, "", err, sizeof(err));
        cout << "\n  Request refused: " << err << "\n";
        return;
    }
    int svc = allowed[readInt("  Choose service: ", 1, n) - 1];
    cout << "  Urgency: 1) Urgent   2) Normal   3) Low\n";
    int urgency = readInt("  Choose urgency (1-3): ", 1, 3);
    char note[NOTE_LEN];
    readLine("  Service notes (recipient/location info): ", note, sizeof(note));

    char err[200];
    if (createRequestRecord(ctx, parcelId, svc, urgency, note, err, sizeof(err))) {
        cout << "\n  Service request created for parcel " << parcelId << " and queued ("
             << ctx.pending.size() << " pending).\n";
    } else {
        cout << "\n  Request refused: " << err << "\n";
    }
}

static void displayActiveMenu(SystemContext& ctx) {
    displayPendingRequests(ctx);
    displayInProgress(ctx);
}

static void showNextMenu(SystemContext& ctx) {
    printHeading("Next Request to Be Handled");
    ServiceRequest r;
    if (!ctx.pending.peek(r)) { cout << "  No pending requests.\n"; return; }
    printRequestDetails(r);
    cout << "\n  Rule: lowest urgency number first (1 Urgent, 2 Normal, 3 Low); equal urgency -> earliest request.\n"
         << "  If this request's resource type is fully busy, the next-best request that can start is taken instead.\n";
}

static void completeMenu(SystemContext& ctx) {
    printHeading("Complete an In-Progress Request");
    displayInProgress(ctx);
    if (ctx.inProgress.size() == 0) return;
    char id[ID_LEN];
    readLine("\n  Request ID to complete (blank = cancel): ", id, sizeof(id));
    normalizeId(id);
    if (id[0] == '\0') return;
    char err[200];
    if (completeRequestRecord(ctx, id, err, sizeof(err))) cout << "\n  Request " << id << " completed. Parcel and resource updated.\n";
    else cout << "\n  Cannot complete: " << err << "\n";
}

static void cancelMenu(SystemContext& ctx) {
    printHeading("Cancel a Request");
    displayActiveMenu(ctx);
    if (ctx.pending.isEmpty() && ctx.inProgress.size() == 0) return;
    char id[ID_LEN];
    readLine("\n  Request ID to cancel (blank = back): ", id, sizeof(id));
    normalizeId(id);
    if (id[0] == '\0') return;
    if (!confirm("  Cancel this request? (y/n): ")) { cout << "  Not cancelled.\n"; return; }
    char err[200];
    if (cancelRequestRecord(ctx, id, err, sizeof(err))) cout << "\n  Request " << id << " cancelled.\n";
    else cout << "\n  Cannot cancel: " << err << "\n";
}

static void searchRequestMenu(SystemContext& ctx) {
    printHeading("Search for a Request");
    char id[ID_LEN];
    readLine("  Request ID: ", id, sizeof(id));
    normalizeId(id);
    ServiceRequest r;
    int i;
    cout << "\n";
    if (ctx.pending.getById(id, r))               printRequestDetails(r);
    else if ((i = findInProgress(ctx, id)) >= 0)  printRequestDetails(ctx.inProgress.at(i));
    else if ((i = findArchived(ctx, id)) >= 0)    printRequestDetails(ctx.archive.at(i));
    else cout << "  Request '" << id << "' was not found.\n";
}

static void viewArchiveMenu(SystemContext& ctx) {
    printHeading("Completed and Cancelled Requests");
    if (ctx.archive.size() == 0) { cout << "  Nothing has been completed or cancelled yet.\n"; return; }
    printRequestHeader();
    for (int i = 0; i < ctx.archive.size(); i++) printRequestRow(ctx.archive.at(i));
}

void requestMenu(SystemContext& ctx) {
    while (true) {
        printHeading("TASK 2 - Parcel Service and Collection Management");
        cout << "  1. Create a service request\n"
             << "  2. Display active requests (pending and in progress)\n"
             << "  3. Show the next request to be handled\n"
             << "  4. Process next request (assigns a counter/courier)\n"
             << "  5. Complete an in-progress request\n"
             << "  6. Cancel a request\n"
             << "  7. Search for a request by ID\n"
             << "  8. View completed / cancelled requests\n"
             << "  0. Back to main menu\n";
        switch (readInt("Choose (0-8): ", 0, 8)) {
            case 1: createRequestMenu(ctx);  break;
            case 2: displayActiveMenu(ctx);  break;
            case 3: showNextMenu(ctx);       break;
            case 4: printHeading("Process Next Request"); startRequest(ctx, nullptr, nullptr); break;
            case 5: completeMenu(ctx);       break;
            case 6: cancelMenu(ctx);         break;
            case 7: searchRequestMenu(ctx);  break;
            case 8: viewArchiveMenu(ctx);    break;
            case 0: return;
        }
    }
}
