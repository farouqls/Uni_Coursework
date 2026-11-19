// main.cpp
// Smart Campus Parcel Collection and Delivery Management System
// ONE integrated prototype: the four task modules share a single SystemContext,
// so a parcel registered in Task 1 flows through Tasks 2, 3 and 4.
//
//   Task 1  Parcel Intake, Sorting and Processing    BST + Queue
//   Task 2  Service and Collection Management        Priority Queue
//   Task 3  Counter and Delivery Resource Management Circular Queue
//   Task 4  Activity and Record Management           Stack
#include <iostream>
#include <cstdio>
#include "System.hpp"
#include "Utils.hpp"
#include "IntakeModule.hpp"
#include "RequestModule.hpp"
#include "ResourceModule.hpp"
#include "ActivityModule.hpp"

using namespace std;

// ---------------------------- dashboard ----------------------------
static void countVisitor(const Parcel& p, void* user) { ((int*)user)[p.status]++; }

static void showDashboard(SystemContext& ctx) {
    printHeading("System Dashboard");
    int byStatus[PS_STATUS_COUNT] = { 0 };
    ctx.registry.inorder(countVisitor, byStatus);
    cout << "  Parcels registered : " << ctx.registry.size() << "   (BST height " << ctx.registry.height() << ")\n";
    for (int s = 0; s < PS_STATUS_COUNT; s++)
        if (byStatus[s] > 0) cout << "      " << parcelStatusName(s) << ": " << byStatus[s] << "\n";
    cout << "  Intake queue       : " << ctx.intake.size() << " waiting\n";
    cout << "  Service requests   : " << ctx.pending.size() << " pending, " << ctx.inProgress.size()
         << " in progress, " << ctx.archive.size() << " completed/cancelled\n";
    int avail[RT_COUNT] = { 0, 0 }, total[RT_COUNT] = { 0, 0 };
    for (int i = 0; i < ctx.resourceCount; i++) {
        total[ctx.resources[i].type]++;
        if (ctx.resources[i].status == RES_AVAILABLE) avail[ctx.resources[i].type]++;
    }
    cout << "  Counters free      : " << avail[RT_COUNTER] << " of " << total[RT_COUNTER] << "\n";
    cout << "  Couriers free      : " << avail[RT_COURIER] << " of " << total[RT_COURIER] << "\n";
    cout << "  Activities logged  : " << ctx.activities.size() << "\n";
}

// ---------------------------- guided demo ----------------------------
struct IdList { char ids[64][ID_LEN]; int count; };
static void collectIds(const Parcel& p, void* user) {
    IdList* list = (IdList*)user;
    if (list->count < 64) copyStr(list->ids[list->count++], ID_LEN, p.id);
}

static int serviceForStatus(int status) {
    switch (status) {
        case PS_READY_COLLECTION:  return SVC_COLLECTION;
        case PS_AWAITING_DELIVERY: return SVC_CAMPUS_DELIVERY;
        case PS_SPECIAL_HANDLING:  return SVC_SPECIAL_HANDLING;
        default:                   return SVC_RETURN;
    }
}

// Walks one parcel set through every module so the integration can be seen end to end.
static void runGuidedDemo(SystemContext& ctx) {
    printHeading("GUIDED DEMO - parcels flowing through all four modules");

    cout << "\nSTEP 1 (Task 1): load the sample parcels into the BST and the intake queue\n";
    if (ctx.registry.isEmpty()) {
        int skipped = 0;
        int n = loadParcelsFromCsv(ctx, "sample_parcels.csv", &skipped);
        if (n < 0) { cout << "  sample_parcels.csv not found - keep it beside the program.\n"; return; }
        cout << "  Loaded " << n << " parcels.\n";
    } else {
        cout << "  Parcels already registered (" << ctx.registry.size() << "); using them.\n";
    }
    cout << "  Waiting in the intake queue: " << ctx.intake.size() << "\n";

    cout << "\nSTEP 2 (Task 1): sort every waiting parcel (queue order, suggested action)\n";
    autoSortAllParcels(ctx);

    cout << "\nSTEP 3 (Task 2): raise a service request per sorted parcel, mixed urgencies\n";
    IdList list; list.count = 0;
    ctx.registry.inorder(collectIds, &list);
    int made = 0;
    for (int i = 0; i < list.count && made < 8; i++) {
        Parcel* p = ctx.registry.find(list.ids[i]);
        if (p == nullptr || !canRequestService(p->status) || parcelHasActiveRequest(ctx, p->id)) continue;
        char err[200];
        int urgency = (made % 3) + 1;
        if (createRequestRecord(ctx, p->id, serviceForStatus(p->status), urgency, "Demo request", err, sizeof(err))) made++;
    }
    cout << "  Created " << made << " requests.\n";
    displayPendingRequests(ctx);

    cout << "\nSTEP 4 (Tasks 2 + 3): process the next requests - each takes the next counter/courier in rotation\n";
    for (int i = 0; i < 5; i++) if (!startRequest(ctx, nullptr, nullptr)) break;
    displayResources(ctx);

    cout << "\nSTEP 5 (Task 4): undo the most recent reversible activity (the last request start)\n";
    reverseLatestActivity(ctx);
    cout << "  Re-starting the next request:\n";
    startRequest(ctx, nullptr, nullptr);

    cout << "\nSTEP 6 (Task 2): complete one in-progress request and cancel another\n";
    char err[200];
    if (ctx.inProgress.size() > 0) {
        char id[ID_LEN]; copyStr(id, sizeof(id), ctx.inProgress.at(0).id);
        if (completeRequestRecord(ctx, id, err, sizeof(err))) cout << "  " << id << " completed.\n";
    }
    if (ctx.inProgress.size() > 0) {
        char id[ID_LEN]; copyStr(id, sizeof(id), ctx.inProgress.at(0).id);
        if (cancelRequestRecord(ctx, id, err, sizeof(err))) cout << "  " << id << " cancelled (resource freed, parcel restored).\n";
    }
    displayResources(ctx);

    cout << "\nSTEP 7 (Task 4): the stack of recent activities, newest on top\n";
    showRecentActivities(ctx, 8);
    cout << "\nDemo finished. Explore any module from the main menu - everything is live.\n";
}

// ---------------------------- main ----------------------------
static void printMainMenu() {
    printHeading("SMART CAMPUS PARCEL COLLECTION AND DELIVERY MANAGEMENT SYSTEM");
    cout << "  1. Parcel Intake, Sorting and Processing          (Task 1)\n"
         << "  2. Parcel Service and Collection Management       (Task 2)\n"
         << "  3. Counter and Delivery Resource Management       (Task 3)\n"
         << "  4. Parcel Activity and Record Management          (Task 4)\n"
         << "  5. System dashboard\n"
         << "  6. Run the guided demo (all modules together)\n"
         << "  0. Exit\n";
}

int main() {
    SystemContext ctx;

    // Starting resources so the system is usable immediately.
    addResource(ctx, RT_COUNTER, "Counter 1", false);
    addResource(ctx, RT_COUNTER, "Counter 2", false);
    addResource(ctx, RT_COUNTER, "Counter 3", false);
    addResource(ctx, RT_COURIER, "Courier Ahmad", false);
    addResource(ctx, RT_COURIER, "Courier Bala", false);
    addResource(ctx, RT_COURIER, "Courier Chong", false);

    bool running = true;
    while (running) {
        printMainMenu();
        switch (readInt("Choose (0-6): ", 0, 6)) {
            case 1: intakeMenu(ctx);     break;
            case 2: requestMenu(ctx);    break;
            case 3: resourceMenu(ctx);   break;
            case 4: activityMenu(ctx);   break;
            case 5: showDashboard(ctx);  break;
            case 6: runGuidedDemo(ctx);  break;
            case 0: running = false;     break;
        }
    }
    cout << "\nGoodbye.\n";
    return 0;
}
