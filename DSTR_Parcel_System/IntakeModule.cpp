// IntakeModule.cpp  -- TASK 1: Parcel Intake, Sorting and Processing
//
// Data structures used here:
//   * BST   (ctx.registry) : the parcel registry - search / update / remove by Parcel ID
//   * Queue (ctx.intake)   : parcels waiting to be sorted, first come first served
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include "IntakeModule.hpp"
#include "RequestModule.hpp"
#include "ActivityModule.hpp"
#include "Utils.hpp"

using namespace std;

// ============================================================
//  Status changes (the one place a parcel's status is modified)
// ============================================================
bool changeParcelStatus(SystemContext& ctx, const char* parcelId, int newStatus,
                        const char* description, bool logIt, bool reversible) {
    Parcel* p = ctx.registry.find(parcelId);
    if (p == nullptr) {
        cout << "  Error: invalid Parcel ID '" << parcelId << "'.\n";
        return false;
    }
    if (isTerminalStatus(p->status)) {
        cout << "  Error: parcel " << parcelId << " is already completed ("
             << parcelStatusName(p->status) << ") and cannot be processed again.\n";
        return false;
    }
    if (!isValidTransition(p->status, newStatus)) {
        cout << "  Error: a parcel cannot go from '" << parcelStatusName(p->status)
             << "' to '" << parcelStatusName(newStatus) << "'.\n";
        return false;
    }
    int oldStatus = p->status;
    p->status = newStatus;
    if (logIt)
        logActivity(ctx, AT_PARCEL_STATUS, parcelId, "", "", oldStatus, newStatus, description, reversible);
    return true;
}

// ============================================================
//  Validation and registration
// ============================================================
static bool validateParcel(const Parcel& p, char* err, size_t n) {
    if (p.id[0] == '\0')            { copyStr(err, n, "Parcel ID is missing."); return false; }
    if (p.recipientName[0] == '\0') { copyStr(err, n, "Recipient name is missing."); return false; }
    if (!isValidContact(p.recipientContact)) {
        copyStr(err, n, "Recipient contact must be 7-15 digits (optional leading +).");
        return false;
    }
    if (p.type < 0 || p.type >= PT_TYPE_COUNT)        { copyStr(err, n, "Invalid parcel type."); return false; }
    if (p.deliveryMode < 0 || p.deliveryMode >= DM_COUNT) { copyStr(err, n, "Invalid delivery mode."); return false; }
    if (p.deliveryMode == DM_DELIVERY &&
        (p.deliveryAddress[0] == '\0' || strcmp(p.deliveryAddress, "-") == 0)) {
        copyStr(err, n, "A delivery address is required for campus delivery.");
        return false;
    }
    if (p.arrival[0] == '\0')       { copyStr(err, n, "Arrival date/time is missing."); return false; }
    return true;
}

// Finds the next unused Parcel ID WITHOUT consuming it, so a failed registration leaves no gap.
static long peekParcelId(SystemContext& ctx, char* out, size_t n) {
    long candidate = ctx.nextParcelNo;
    do {
        snprintf(out, n, "PCL%03ld", candidate);
        if (ctx.registry.find(out) == nullptr) break;   // never reuse an existing ID
        candidate++;
    } while (true);
    return candidate;
}

bool registerParcelRecord(SystemContext& ctx, Parcel& p, char* err, size_t errSize) {
    if (!validateParcel(p, err, errSize)) return false;
    if (!ctx.registry.insert(p)) {
        snprintf(err, errSize, "Duplicate Parcel ID '%s': a parcel with this ID already exists.", p.id);
        return false;
    }
    ctx.intake.enqueue(p.id);                           // waits its turn for sorting
    char desc[DESC_LEN];
    snprintf(desc, sizeof(desc), "Registered parcel for %s (%s, %s)",
             p.recipientName, parcelTypeName(p.type), deliveryModeName(p.deliveryMode));
    logActivity(ctx, AT_PARCEL_REGISTERED, p.id, "", "", -1, PS_REGISTERED, desc, true);
    return true;
}

// ============================================================
//  Sorting rules
// ============================================================
// Suggested next action. Staff may accept it or override it.
int suggestStatus(const Parcel& p, const char** reasonOut) {
    if (p.recipientName[0] == '\0' || !isValidContact(p.recipientContact)) {
        *reasonOut = "recipient information is incomplete";
        return PS_ON_HOLD;
    }
    if (p.type == PT_FRAGILE) {
        *reasonOut = "fragile parcels need special handling";
        return PS_SPECIAL_HANDLING;
    }
    if (p.deliveryMode == DM_DELIVERY) {
        *reasonOut = "recipient asked for campus delivery";
        return PS_AWAITING_DELIVERY;
    }
    *reasonOut = "recipient will collect at the counter";
    return PS_READY_COLLECTION;
}

static int classifyChoiceToStatus(int choice) {
    switch (choice) {
        case 1: return PS_READY_COLLECTION;
        case 2: return PS_AWAITING_DELIVERY;
        case 3: return PS_SPECIAL_HANDLING;
        case 4: return PS_RETURN_PENDING;
        default: return PS_ON_HOLD;
    }
}

// Non-interactive: sort every waiting parcel using the suggested action (used by the demo).
int autoSortAllParcels(SystemContext& ctx) {
    int sorted = 0;
    char id[ID_LEN];
    while (ctx.intake.dequeue(id)) {
        Parcel* p = ctx.registry.find(id);
        if (p == nullptr || p->status != PS_REGISTERED) continue;
        const char* reason = "";
        int target = suggestStatus(*p, &reason);
        char desc[DESC_LEN];
        snprintf(desc, sizeof(desc), "Sorted: %s (%s)", parcelStatusName(target), reason);
        if (changeParcelStatus(ctx, id, target, desc, true, true)) {
            cout << "  " << id << " -> " << parcelStatusName(target) << "  (" << reason << ")\n";
            sorted++;
        }
    }
    return sorted;
}

// ============================================================
//  CSV loading
// ============================================================
static bool parseCsvRow(char* line, Parcel& p, char* err, size_t n) {
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) line[--len] = '\0';
    if (len == 0) { copyStr(err, n, "blank line"); return false; }

    char* fields[7];
    int count = 0;
    char* cursor = line;
    while (count < 7) {
        fields[count++] = cursor;
        char* comma = strchr(cursor, ',');
        if (comma == nullptr) break;
        *comma = '\0';
        cursor = comma + 1;
    }
    if (count != 7) { copyStr(err, n, "expected 7 columns"); return false; }
    for (int i = 0; i < 7; i++) trimInPlace(fields[i]);

    p = Parcel();
    copyStr(p.id, sizeof(p.id), fields[0]);          normalizeId(p.id);
    copyStr(p.recipientName, sizeof(p.recipientName), fields[1]);
    copyStr(p.recipientContact, sizeof(p.recipientContact), fields[2]);
    p.type = -1;
    for (int t = 0; t < PT_TYPE_COUNT; t++) if (equalsIgnoreCase(fields[3], parcelTypeName(t))) p.type = t;
    p.deliveryMode = -1;
    for (int m = 0; m < DM_COUNT; m++) if (equalsIgnoreCase(fields[4], deliveryModeName(m))) p.deliveryMode = m;
    copyStr(p.deliveryAddress, sizeof(p.deliveryAddress), fields[5][0] ? fields[5] : "-");
    if (fields[6][0] != '\0') copyStr(p.arrival, sizeof(p.arrival), fields[6]);
    else getTimestamp(p.arrival, sizeof(p.arrival));
    p.status = PS_REGISTERED;
    return true;
}

int loadParcelsFromCsv(SystemContext& ctx, const char* filename, int* skipped) {
    ifstream file(filename);
    if (!file.is_open()) return -1;
    char line[300];
    int loaded = 0, bad = 0, lineNo = 1;
    file.getline(line, sizeof(line));                  // skip the header row
    while (file.getline(line, sizeof(line))) {
        lineNo++;
        Parcel p;
        char err[160];
        if (!parseCsvRow(line, p, err, sizeof(err)) || !registerParcelRecord(ctx, p, err, sizeof(err))) {
            cout << "  Line " << lineNo << " skipped: " << err << "\n";
            bad++;
        } else {
            loaded++;
        }
    }
    if (skipped != nullptr) *skipped = bad;
    return loaded;
}

// ============================================================
//  Menu actions
// ============================================================
static void registerParcelMenu(SystemContext& ctx) {
    printHeading("Register a New Parcel");
    Parcel p = Parcel();
    long idNumber = peekParcelId(ctx, p.id, sizeof(p.id));
    cout << "  Assigned Parcel ID: " << p.id << "\n";
    readLine("  Recipient name            : ", p.recipientName, sizeof(p.recipientName));
    readLine("  Recipient contact (phone) : ", p.recipientContact, sizeof(p.recipientContact));
    cout << "  Parcel type: 1) Standard  2) Fragile  3) Document  4) Oversized\n";
    p.type = readInt("  Choose type (1-4): ", 1, 4) - 1;
    cout << "  Delivery mode: 1) Recipient collects at counter  2) Campus delivery\n";
    p.deliveryMode = readInt("  Choose mode (1-2): ", 1, 2) - 1;
    if (p.deliveryMode == DM_DELIVERY) readLine("  Delivery address          : ", p.deliveryAddress, sizeof(p.deliveryAddress));
    else copyStr(p.deliveryAddress, sizeof(p.deliveryAddress), "-");
    getTimestamp(p.arrival, sizeof(p.arrival));
    p.status = PS_REGISTERED;

    char err[160];
    if (registerParcelRecord(ctx, p, err, sizeof(err))) {
        ctx.nextParcelNo = idNumber + 1;                // only now is the ID used up
        cout << "\n  Parcel " << p.id << " registered and added to the intake queue (position "
             << ctx.intake.size() << ").\n";
    } else
        cout << "\n  Registration failed: " << err << "\n";
}

static void printRowVisitor(const Parcel& p, void*) { printParcelRow(p); }

static void displayAllParcels(SystemContext& ctx) {
    printHeading("All Parcels (sorted by Parcel ID - BST in-order walk)");
    if (ctx.registry.isEmpty()) { cout << "  No parcels have been registered yet.\n"; return; }
    printParcelHeader();
    ctx.registry.inorder(printRowVisitor, nullptr);
    cout << "\n  Total parcels: " << ctx.registry.size() << "   |   BST height: " << ctx.registry.height() << "\n";
}

struct NameSearch { const char* needle; int found; };
static void nameVisitor(const Parcel& p, void* user) {
    NameSearch* s = (NameSearch*)user;
    if (containsIgnoreCase(p.recipientName, s->needle)) {
        if (s->found == 0) printParcelHeader();
        printParcelRow(p);
        s->found++;
    }
}

static void searchParcelMenu(SystemContext& ctx) {
    printHeading("Search for a Parcel");
    if (ctx.registry.isEmpty()) { cout << "  There are no parcels to search.\n"; return; }
    cout << "  1) By Parcel ID   2) By recipient name\n";
    int mode = readInt("  Choose (1-2): ", 1, 2);
    if (mode == 1) {
        char id[ID_LEN];
        readLine("  Parcel ID: ", id, sizeof(id));
        normalizeId(id);
        Parcel* p = ctx.registry.find(id);
        if (p == nullptr) { cout << "\n  Parcel '" << id << "' was not found.\n"; return; }
        cout << "\n";
        printParcelDetails(*p);
        cout << "  Active request : " << (parcelHasActiveRequest(ctx, id) ? "Yes" : "No") << "\n";
    } else {
        char name[NAME_LEN];
        readLine("  Name (or part of a name): ", name, sizeof(name));
        if (name[0] == '\0') { cout << "  Nothing entered.\n"; return; }
        NameSearch s = { name, 0 };
        cout << "\n";
        ctx.registry.inorder(nameVisitor, &s);
        if (s.found == 0) cout << "  No parcel matches that name.\n";
        else cout << "\n  " << s.found << " parcel(s) found.\n";
    }
}

static void updateParcelMenu(SystemContext& ctx) {
    printHeading("Update Parcel Details");
    char id[ID_LEN];
    readLine("  Parcel ID: ", id, sizeof(id));
    normalizeId(id);
    Parcel* p = ctx.registry.find(id);
    if (p == nullptr) { cout << "\n  Update failed: parcel '" << id << "' does not exist.\n"; return; }
    if (isTerminalStatus(p->status)) {
        cout << "\n  Update refused: the parcel is already " << parcelStatusName(p->status) << ".\n";
        return;
    }
    cout << "\n";
    printParcelDetails(*p);
    cout << "\n  Edit: 1) Recipient name  2) Contact  3) Delivery address  4) Delivery mode  0) Cancel\n";
    int field = readInt("  Choose (0-4): ", 0, 4);
    if (field == 0) return;

    Parcel edited = *p;                                  // validate on a copy first
    if (field == 1) readLine("  New recipient name: ", edited.recipientName, sizeof(edited.recipientName));
    if (field == 2) readLine("  New contact: ", edited.recipientContact, sizeof(edited.recipientContact));
    if (field == 3) readLine("  New delivery address: ", edited.deliveryAddress, sizeof(edited.deliveryAddress));
    if (field == 4) {
        cout << "  1) Recipient collects  2) Campus delivery\n";
        edited.deliveryMode = readInt("  Choose (1-2): ", 1, 2) - 1;
        if (edited.deliveryMode == DM_COLLECTION) copyStr(edited.deliveryAddress, sizeof(edited.deliveryAddress), "-");
    }
    char err[160];
    if (!validateParcel(edited, err, sizeof(err))) { cout << "\n  Update rejected: " << err << "\n"; return; }
    *p = edited;
    char desc[DESC_LEN];
    snprintf(desc, sizeof(desc), "Updated details of parcel %s", id);
    logActivity(ctx, AT_PARCEL_UPDATED, id, "", "", -1, -1, desc, false);
    cout << "\n  Parcel " << id << " updated.\n";
}

static void removeParcelMenu(SystemContext& ctx) {
    printHeading("Remove a Parcel Record");
    char id[ID_LEN];
    readLine("  Parcel ID: ", id, sizeof(id));
    normalizeId(id);
    Parcel* p = ctx.registry.find(id);
    if (p == nullptr) { cout << "\n  Removal failed: parcel '" << id << "' does not exist.\n"; return; }
    if (parcelHasActiveRequest(ctx, id) || p->status == PS_IN_SERVICE) {
        cout << "\n  Removal refused: the parcel has an active service request. Complete or cancel it first.\n";
        return;
    }
    cout << "\n";
    printParcelDetails(*p);
    if (!confirm("\n  Permanently remove this parcel? (y/n): ")) { cout << "  Cancelled.\n"; return; }
    ctx.intake.removeById(id);                           // no-op if it was not waiting
    ctx.registry.remove(id);
    char desc[DESC_LEN];
    snprintf(desc, sizeof(desc), "Removed parcel %s from the registry", id);
    logActivity(ctx, AT_PARCEL_REMOVED, id, "", "", -1, -1, desc, false);
    cout << "  Parcel " << id << " removed.\n";
}

static void queueVisitor(const char* id, int position, void* user) {
    SystemContext* ctx = (SystemContext*)user;
    Parcel* p = ctx->registry.find(id);
    char num[12];
    snprintf(num, sizeof(num), "%d", position);
    printPad(num, 4);
    if (p != nullptr) { printPad(p->id, 8); printPad(p->recipientName, 20); printPad(parcelTypeName(p->type), 10); printPad(deliveryModeName(p->deliveryMode), 11); }
    else printPad(id, 8);
    cout << "\n";
}

static void viewIntakeQueue(SystemContext& ctx) {
    printHeading("Intake Queue (waiting to be sorted, front first)");
    if (ctx.intake.isEmpty()) { cout << "  The intake queue is empty.\n"; return; }
    printPad("No.", 4); printPad("ID", 8); printPad("Recipient", 20); printPad("Type", 10); printPad("Mode", 11);
    cout << "\n";
    printRule(56);
    ctx.intake.forEach(queueVisitor, &ctx);
    cout << "\n  " << ctx.intake.size() << " parcel(s) waiting.\n";
}

static void processNextParcel(SystemContext& ctx) {
    printHeading("Process Next Waiting Parcel (sort for its next action)");
    if (ctx.intake.isEmpty()) { cout << "  There are no parcels waiting to be processed (empty queue).\n"; return; }

    char id[ID_LEN];
    ctx.intake.dequeue(id);
    Parcel* p = ctx.registry.find(id);
    if (p == nullptr) { cout << "  Queue held an unknown parcel '" << id << "'; it was discarded.\n"; return; }
    if (p->status != PS_REGISTERED) {
        cout << "  Parcel " << id << " was already processed (" << parcelStatusName(p->status) << ").\n";
        return;
    }
    printParcelDetails(*p);
    const char* reason = "";
    int suggested = suggestStatus(*p, &reason);
    cout << "\n  Suggested action: " << parcelStatusName(suggested) << "  (" << reason << ")\n";
    cout << "  0) Accept suggestion   1) Ready for collection   2) Campus delivery\n"
         << "  3) Special handling    4) Return to sender       5) Hold for investigation\n"
         << "  6) Skip - put back at the front of the queue\n";
    int choice = readInt("  Choose (0-6): ", 0, 6);
    if (choice == 6) {
        ctx.intake.enqueueFront(id);
        cout << "  Parcel " << id << " returned to the front of the queue.\n";
        return;
    }
    int target = (choice == 0) ? suggested : classifyChoiceToStatus(choice);
    char desc[DESC_LEN];
    snprintf(desc, sizeof(desc), "Sorted: %s%s", parcelStatusName(target), choice == 0 ? " (suggested)" : " (staff choice)");
    if (changeParcelStatus(ctx, id, target, desc, true, true))
        cout << "\n  Parcel " << id << " is now '" << parcelStatusName(target) << "' and routed to the next stage.\n";
    else
        ctx.intake.enqueueFront(id);                     // keep its place if something went wrong
}

static void resortHeldParcel(SystemContext& ctx) {
    printHeading("Re-sort a Parcel That Is On Hold");
    char id[ID_LEN];
    readLine("  Parcel ID: ", id, sizeof(id));
    normalizeId(id);
    Parcel* p = ctx.registry.find(id);
    if (p == nullptr) { cout << "\n  Parcel '" << id << "' does not exist.\n"; return; }
    if (p->status != PS_ON_HOLD) {
        cout << "\n  Parcel " << id << " is '" << parcelStatusName(p->status) << "', not On Hold.\n";
        return;
    }
    cout << "  1) Ready for collection   2) Campus delivery   3) Special handling   4) Return to sender\n";
    int choice = readInt("  Choose (1-4): ", 1, 4);
    int target = classifyChoiceToStatus(choice);
    char desc[DESC_LEN];
    snprintf(desc, sizeof(desc), "Re-sorted from hold: %s", parcelStatusName(target));
    if (changeParcelStatus(ctx, id, target, desc, true, true))
        cout << "\n  Parcel " << id << " is now '" << parcelStatusName(target) << "'.\n";
}

static void missingMenu(SystemContext& ctx) {
    printHeading("Report a Parcel Missing / Found");
    cout << "  1) Report parcel as missing   2) Mark a missing parcel as found (puts it On Hold)\n";
    int mode = readInt("  Choose (1-2): ", 1, 2);
    char id[ID_LEN];
    readLine("  Parcel ID: ", id, sizeof(id));
    normalizeId(id);
    Parcel* p = ctx.registry.find(id);
    if (p == nullptr) { cout << "\n  Parcel '" << id << "' does not exist.\n"; return; }
    if (mode == 1) {
        if (parcelHasActiveRequest(ctx, id)) {
            cout << "\n  Refused: cancel the active service request for " << id << " first.\n";
            return;
        }
        bool wasWaiting = (p->status == PS_REGISTERED);
        if (changeParcelStatus(ctx, id, PS_MISSING, "Reported missing", true, true)) {
            if (wasWaiting) ctx.intake.removeById(id);  // a missing parcel must not stay in the queue
            cout << "\n  Parcel " << id << " reported missing.\n";
        }
    } else {
        if (changeParcelStatus(ctx, id, PS_ON_HOLD, "Found again - held for re-sorting", true, true))
            cout << "\n  Parcel " << id << " found and placed On Hold. Re-sort it from the menu.\n";
    }
}

static void loadCsvMenu(SystemContext& ctx) {
    printHeading("Load Parcels from a CSV File");
    char filename[100];
    readLine("  File name (Enter for sample_parcels.csv): ", filename, sizeof(filename));
    if (filename[0] == '\0') copyStr(filename, sizeof(filename), "sample_parcels.csv");
    int skipped = 0;
    int loaded = loadParcelsFromCsv(ctx, filename, &skipped);
    if (loaded < 0) { cout << "\n  Could not open '" << filename << "'. Keep it in the same folder as the program.\n"; return; }
    cout << "\n  Loaded " << loaded << " parcel(s); skipped " << skipped << ".\n";
}

void intakeMenu(SystemContext& ctx) {
    while (true) {
        printHeading("TASK 1 - Parcel Intake, Sorting and Processing");
        cout << "   1. Register a new parcel\n"
             << "   2. Display all parcels\n"
             << "   3. Search for a parcel\n"
             << "   4. Update parcel details\n"
             << "   5. Remove a parcel\n"
             << "   6. View the intake queue\n"
             << "   7. Process next waiting parcel (sort)\n"
             << "   8. Re-sort a parcel that is on hold\n"
             << "   9. Report a parcel missing / found\n"
             << "  10. Load parcels from a CSV file\n"
             << "   0. Back to main menu\n";
        switch (readInt("Choose (0-10): ", 0, 10)) {
            case 1: registerParcelMenu(ctx); break;
            case 2: displayAllParcels(ctx);  break;
            case 3: searchParcelMenu(ctx);   break;
            case 4: updateParcelMenu(ctx);   break;
            case 5: removeParcelMenu(ctx);   break;
            case 6: viewIntakeQueue(ctx);    break;
            case 7: processNextParcel(ctx);  break;
            case 8: resortHeldParcel(ctx);   break;
            case 9: missingMenu(ctx);        break;
            case 10: loadCsvMenu(ctx);       break;
            case 0: return;
        }
    }
}
