// ActivityModule.cpp  -- TASK 4: Parcel Activity and Record Management
//
// Data structure used here:
//   * Stack (ctx.activities): every important activity is pushed; the newest is on top.
//     Undo works backwards from the newest. History is never erased - reversing an
//     activity marks it 'reversed' and pushes a Reversal entry.
//
// Reversal rules (all checked before anything changes):
//   1. the activity type must be reversible and not already reversed
//   2. it must be the newest still-active activity of that parcel (undo in LIFO order)
//   3. the parcel/request/resource must still be in the state the activity left it in
#include <iostream>
#include <cstdio>
#include <cstring>
#include "ActivityModule.hpp"
#include "ResourceModule.hpp"
#include "Utils.hpp"

using namespace std;

void logActivity(SystemContext& ctx, int type, const char* parcelId, const char* requestId,
                 const char* resourceId, int oldStatus, int newStatus,
                 const char* description, bool reversible) {
    Activity a = Activity();
    a.seq = ctx.nextActivitySeq++;
    getTimestamp(a.timestamp, sizeof(a.timestamp));
    a.type = type;
    copyStr(a.parcelId, sizeof(a.parcelId), parcelId);
    copyStr(a.requestId, sizeof(a.requestId), requestId);
    copyStr(a.resourceId, sizeof(a.resourceId), resourceId);
    a.oldStatus = oldStatus;
    a.newStatus = newStatus;
    copyStr(a.description, sizeof(a.description), description);
    a.reversible = reversible;
    a.reversed = false;
    ctx.activities.push(a);
}

// ============================ display helpers ============================
static const char* stateTag(const Activity& a) {
    if (a.type == AT_REVERSAL) return "[undo record]";
    if (a.reversed)            return "[REVERSED]";
    return a.reversible ? "[can undo]" : "[final]";
}

static void printActivity(const Activity& a) {
    char seq[16];
    snprintf(seq, sizeof(seq), "#%ld", a.seq);
    printPad(seq, 5); printPad(a.timestamp, 16); printPad(a.parcelId[0] ? a.parcelId : "-", 8);
    printPad(activityTypeName(a.type), 18); cout << stateTag(a) << "\n";
    cout << "      " << a.description;
    if (a.oldStatus >= 0 && a.newStatus >= 0 && a.type != AT_REVERSAL)
        cout << "   [" << parcelStatusName(a.oldStatus) << " -> " << parcelStatusName(a.newStatus) << "]";
    cout << "\n";
}

static bool parcelInHistory(SystemContext& ctx, const char* parcelId) {
    for (int i = 0; i < ctx.activities.size(); i++)
        if (strcmp(ctx.activities.at(i).parcelId, parcelId) == 0) return true;
    return false;
}

void showRecentActivities(SystemContext& ctx, int howMany) {
    printHeading("Recent Activities (newest first)");
    if (ctx.activities.isEmpty()) { cout << "  No activity has been recorded yet.\n"; return; }
    int n = ctx.activities.size() < howMany ? ctx.activities.size() : howMany;
    for (int i = 0; i < n; i++) printActivity(ctx.activities.fromTop(i));
    cout << "\n  Showing " << n << " of " << ctx.activities.size() << " activities.\n";
}

static void showParcelHistory(SystemContext& ctx) {
    printHeading("Activity History of a Parcel (oldest first)");
    char id[ID_LEN];
    readLine("  Parcel ID: ", id, sizeof(id));
    normalizeId(id);
    if (ctx.registry.find(id) == nullptr && !parcelInHistory(ctx, id)) {
        cout << "\n  Error: invalid Parcel ID '" << id << "'.\n";
        return;
    }
    if (!parcelInHistory(ctx, id)) { cout << "\n  Parcel " << id << " exists but has no recorded activity.\n"; return; }
    cout << "\n";
    for (int i = 0; i < ctx.activities.size(); i++)
        if (strcmp(ctx.activities.at(i).parcelId, id) == 0) printActivity(ctx.activities.at(i));
}

static void reviewParcel(SystemContext& ctx) {
    printHeading("Review a Parcel");
    char id[ID_LEN];
    readLine("  Parcel ID: ", id, sizeof(id));
    normalizeId(id);
    Parcel* p = ctx.registry.find(id);
    if (p == nullptr && !parcelInHistory(ctx, id)) { cout << "\n  Error: invalid Parcel ID '" << id << "'.\n"; return; }
    cout << "\n";
    if (p != nullptr) printParcelDetails(*p);
    else cout << "  Parcel " << id << " is no longer in the registry (history only).\n";

    // processing sequence + previous status + most recent activity, from the stack
    cout << "\n  Processing sequence: ";
    int lastStatusFrom = -1, lastIndex = -1, steps = 0;
    for (int i = 0; i < ctx.activities.size(); i++) {
        const Activity& a = ctx.activities.at(i);
        if (strcmp(a.parcelId, id) != 0 || a.reversed || a.type == AT_REVERSAL) continue;
        lastIndex = i;
        if (a.newStatus >= 0 && a.oldStatus >= -1 && (a.type == AT_PARCEL_STATUS || a.type == AT_REQUEST_STARTED ||
                                                       a.type == AT_REQUEST_COMPLETED || a.type == AT_PARCEL_REGISTERED ||
                                                       (a.type == AT_REQUEST_CANCELLED && a.oldStatus >= 0))) {
            if (steps > 0) cout << " -> ";
            cout << parcelStatusName(a.newStatus);
            steps++;
            if (a.oldStatus >= 0) lastStatusFrom = a.oldStatus;
        }
    }
    if (steps == 0) cout << "(none)";
    cout << "\n";
    if (lastStatusFrom >= 0) cout << "  Previous status    : " << parcelStatusName(lastStatusFrom) << "\n";
    if (lastIndex >= 0) {
        cout << "  Most recent activity:\n";
        printActivity(ctx.activities.at(lastIndex));
    }
    // related request / resource
    for (int i = 0; i < ctx.pending.size(); i++)
        if (strcmp(ctx.pending.at(i).parcelId, id) == 0) cout << "  Related request    : " << ctx.pending.at(i).id << " (Pending)\n";
    for (int i = 0; i < ctx.inProgress.size(); i++)
        if (strcmp(ctx.inProgress.at(i).parcelId, id) == 0)
            cout << "  Related request    : " << ctx.inProgress.at(i).id << " (In Progress) using " << ctx.inProgress.at(i).resourceId << "\n";
}

// ============================ reversal ============================
// Newest activity (bottom-index) of this parcel that is neither reversed nor an undo record.
static int latestActiveIndexForParcel(SystemContext& ctx, const char* parcelId) {
    for (int i = ctx.activities.size() - 1; i >= 0; i--) {
        const Activity& a = ctx.activities.at(i);
        if (a.reversed || a.type == AT_REVERSAL) continue;
        if (strcmp(a.parcelId, parcelId) == 0) return i;
    }
    return -1;
}

static bool reverseActivityAt(SystemContext& ctx, int idx) {
    Activity a = ctx.activities.at(idx);                 // work on a copy (the stack may grow below)

    if (a.type == AT_REVERSAL) { cout << "  Error: an undo record cannot itself be reversed.\n"; return false; }
    if (a.reversed)            { cout << "  Error: activity #" << a.seq << " has already been reversed.\n"; return false; }
    if (!a.reversible) {
        cout << "  Error: activity #" << a.seq << " (" << activityTypeName(a.type) << ") cannot be reversed.\n";
        return false;
    }
    int newest = latestActiveIndexForParcel(ctx, a.parcelId);
    if (newest != idx) {
        cout << "  Error: a newer activity (#" << ctx.activities.at(newest).seq << ") exists for parcel "
             << a.parcelId << ". Reverse in last-in-first-out order.\n";
        return false;
    }

    char desc[DESC_LEN];
    switch (a.type) {

    case AT_PARCEL_REGISTERED: {                         // undo = take the parcel out again
        Parcel* p = ctx.registry.find(a.parcelId);
        if (p == nullptr) { cout << "  Error: the parcel no longer exists.\n"; return false; }
        if (p->status != PS_REGISTERED || !ctx.intake.contains(a.parcelId)) {
            cout << "  Error: parcel " << a.parcelId << " has already been processed, so its registration cannot be undone.\n";
            return false;
        }
        ctx.intake.removeById(a.parcelId);
        ctx.registry.remove(a.parcelId);
        snprintf(desc, sizeof(desc), "Reversed #%ld: registration of %s undone (parcel removed)", a.seq, a.parcelId);
        break;
    }

    case AT_PARCEL_STATUS: {                             // undo = restore the previous status
        Parcel* p = ctx.registry.find(a.parcelId);
        if (p == nullptr) { cout << "  Error: the parcel no longer exists.\n"; return false; }
        if (p->status != a.newStatus) {
            cout << "  Error: parcel is now '" << parcelStatusName(p->status) << "' (expected '"
                 << parcelStatusName(a.newStatus) << "'); undoing would give an invalid status.\n";
            return false;
        }
        if (a.oldStatus == PS_REGISTERED) ctx.intake.enqueueFront(a.parcelId);   // regain its place in the line
        p->status = a.oldStatus;
        snprintf(desc, sizeof(desc), "Reversed #%ld: parcel %s restored to %s", a.seq, a.parcelId, parcelStatusName(a.oldStatus));
        break;
    }

    case AT_REQUEST_CREATED: {                           // undo = withdraw the pending request
        ServiceRequest r;
        if (!ctx.pending.removeById(a.requestId, &r)) {
            cout << "  Error: request " << a.requestId << " is no longer pending (already started or cancelled).\n";
            return false;
        }
        r.status = RS_CANCELLED;
        copyStr(r.note, sizeof(r.note), "Voided by undo");
        ctx.archive.add(r);
        snprintf(desc, sizeof(desc), "Reversed #%ld: request %s withdrawn", a.seq, a.requestId);
        break;
    }

    case AT_REQUEST_STARTED: {                           // undo = request back to pending, resource freed, parcel restored
        int pos = -1;
        for (int i = 0; i < ctx.inProgress.size(); i++)
            if (strcmp(ctx.inProgress.at(i).id, a.requestId) == 0) pos = i;
        Parcel* p = ctx.registry.find(a.parcelId);
        if (pos < 0) { cout << "  Error: request " << a.requestId << " is no longer in progress.\n"; return false; }
        if (p == nullptr || p->status != PS_IN_SERVICE) { cout << "  Error: parcel is no longer In Service.\n"; return false; }
        if (!releaseResource(ctx, a.resourceId)) return false;
        ServiceRequest r = ctx.inProgress.at(pos);
        ctx.inProgress.removeAt(pos);
        r.status = RS_PENDING;
        r.resourceId[0] = '\0';
        ctx.pending.insert(r);                           // same sequence number, so it regains its priority place
        p->status = a.oldStatus;
        snprintf(desc, sizeof(desc), "Reversed #%ld: %s back to pending, %s released, parcel %s restored to %s",
                 a.seq, a.requestId, a.resourceId, a.parcelId, parcelStatusName(a.oldStatus));
        break;
    }

    default:
        cout << "  Error: this activity type cannot be reversed.\n";
        return false;
    }

    ctx.activities.at(idx).reversed = true;              // mark BEFORE pushing (push may reallocate)
    logActivity(ctx, AT_REVERSAL, a.parcelId, a.requestId, a.resourceId, a.newStatus, a.oldStatus, desc, false);
    cout << "  " << desc << "\n";
    return true;
}

bool reverseLatestActivity(SystemContext& ctx) {
    for (int i = ctx.activities.size() - 1; i >= 0; i--) {
        const Activity& a = ctx.activities.at(i);
        if (a.reversed || a.type == AT_REVERSAL || !a.reversible) continue;
        cout << "  Newest reversible activity: #" << a.seq << " - " << a.description << "\n";
        return reverseActivityAt(ctx, i);
    }
    cout << "  There is no activity that can be reversed.\n";
    return false;
}

static void reverseSelectedMenu(SystemContext& ctx) {
    printHeading("Reverse an Activity of a Parcel");
    if (ctx.activities.isEmpty()) { cout << "  No activity history exists yet.\n"; return; }
    char id[ID_LEN];
    readLine("  Parcel ID: ", id, sizeof(id));
    normalizeId(id);
    if (!parcelInHistory(ctx, id)) { cout << "\n  Error: invalid Parcel ID '" << id << "' (no history).\n"; return; }
    cout << "\n";
    for (int i = 0; i < ctx.activities.size(); i++)
        if (strcmp(ctx.activities.at(i).parcelId, id) == 0) printActivity(ctx.activities.at(i));
    int seq = readInt("\n  Activity number to reverse, e.g. 5 for #5 (0 = cancel): ", 0, 1000000);
    if (seq == 0) return;
    int idx = -1;
    for (int i = 0; i < ctx.activities.size(); i++)
        if (ctx.activities.at(i).seq == seq && strcmp(ctx.activities.at(i).parcelId, id) == 0) idx = i;
    if (idx < 0) { cout << "\n  Error: invalid activity selection - #" << seq << " does not belong to parcel " << id << ".\n"; return; }
    cout << "\n";
    reverseActivityAt(ctx, idx);
}

static void reverseLatestMenu(SystemContext& ctx) {
    printHeading("Reverse the Most Recent Reversible Activity");
    if (ctx.activities.isEmpty()) { cout << "  No activity history exists yet.\n"; return; }
    if (confirm("  Undo it? (y/n): ")) { cout << "\n"; reverseLatestActivity(ctx); }
}

void activityMenu(SystemContext& ctx) {
    while (true) {
        printHeading("TASK 4 - Parcel Activity and Record Management");
        cout << "  1. View recent activities\n"
             << "  2. View the history of a parcel\n"
             << "  3. Review a parcel (status, previous status, latest activity)\n"
             << "  4. Reverse a selected activity of a parcel\n"
             << "  5. Reverse the most recent reversible activity (undo)\n"
             << "  0. Back to main menu\n";
        switch (readInt("Choose (0-5): ", 0, 5)) {
            case 1: showRecentActivities(ctx, 10); break;
            case 2: showParcelHistory(ctx);        break;
            case 3: reviewParcel(ctx);             break;
            case 4: reverseSelectedMenu(ctx);      break;
            case 5: reverseLatestMenu(ctx);        break;
            case 0: return;
        }
    }
}
