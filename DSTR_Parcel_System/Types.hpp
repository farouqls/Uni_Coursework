// Types.hpp
// Shared record types, enums and name lookups used by ALL modules.
// Keeping them in one place lets the four tasks exchange data consistently.
#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstring>

// ---------- Fixed field sizes (plain char arrays, no STL strings/containers) ----------
const int ID_LEN        = 12;
const int NAME_LEN      = 40;
const int CONTACT_LEN   = 20;
const int ADDRESS_LEN   = 60;
const int NOTE_LEN      = 60;
const int TIME_LEN      = 20;
const int DESC_LEN      = 200;
const int MAX_RESOURCES = 20;

// ============================ PARCEL ============================
enum ParcelType   { PT_STANDARD = 0, PT_FRAGILE, PT_DOCUMENT, PT_OVERSIZED, PT_TYPE_COUNT };
enum DeliveryMode { DM_COLLECTION = 0, DM_DELIVERY, DM_COUNT };

// Life cycle of a parcel. The "pending action" states (READY..RETURN_PENDING) are
// the ones from which a service request can be raised.
enum ParcelStatus {
    PS_REGISTERED = 0,      // arrived and waiting in the intake queue
    PS_READY_COLLECTION,    // sorted: recipient will collect
    PS_AWAITING_DELIVERY,   // sorted: campus delivery
    PS_SPECIAL_HANDLING,    // sorted: needs special handling
    PS_RETURN_PENDING,      // sorted: to be returned to sender
    PS_ON_HOLD,             // held for investigation
    PS_IN_SERVICE,          // a resource is working on it right now
    PS_COLLECTED,           // final
    PS_DELIVERED,           // final
    PS_RETURNED,            // final
    PS_MISSING,
    PS_STATUS_COUNT
};

struct Parcel {
    char id[ID_LEN];                    // e.g. "PCL001" (BST key)
    char recipientName[NAME_LEN];
    char recipientContact[CONTACT_LEN];
    char deliveryAddress[ADDRESS_LEN];  // "-" when the recipient collects
    int  type;                          // ParcelType
    int  deliveryMode;                  // DeliveryMode
    char arrival[TIME_LEN];             // "YYYY-MM-DD HH:MM"
    int  status;                        // ParcelStatus
};

inline const char* parcelTypeName(int t) {
    static const char* n[PT_TYPE_COUNT] = { "Standard", "Fragile", "Document", "Oversized" };
    return (t >= 0 && t < PT_TYPE_COUNT) ? n[t] : "Unknown";
}
inline const char* deliveryModeName(int m) {
    static const char* n[DM_COUNT] = { "Collection", "Delivery" };
    return (m >= 0 && m < DM_COUNT) ? n[m] : "Unknown";
}
inline const char* parcelStatusName(int s) {
    static const char* n[PS_STATUS_COUNT] = {
        "Registered (Waiting)", "Ready for Collection", "Awaiting Delivery", "Special Handling",
        "Return to Sender", "On Hold", "In Service", "Collected", "Delivered", "Returned", "Missing"
    };
    return (s >= 0 && s < PS_STATUS_COUNT) ? n[s] : "None";
}

inline bool isTerminalStatus(int s) {
    return s == PS_COLLECTED || s == PS_DELIVERED || s == PS_RETURNED;
}
// States from which a service request may be raised.
inline bool canRequestService(int s) {
    return s == PS_READY_COLLECTION || s == PS_AWAITING_DELIVERY ||
           s == PS_SPECIAL_HANDLING || s == PS_RETURN_PENDING;
}

// The rules that prevent an invalid parcel status. Every status change goes through this.
inline bool isValidTransition(int from, int to) {
    if (from == to) return false;
    switch (from) {
        case PS_REGISTERED:
            return to == PS_READY_COLLECTION || to == PS_AWAITING_DELIVERY || to == PS_SPECIAL_HANDLING ||
                   to == PS_RETURN_PENDING || to == PS_ON_HOLD || to == PS_MISSING;
        case PS_READY_COLLECTION:
        case PS_AWAITING_DELIVERY:
        case PS_SPECIAL_HANDLING:
        case PS_RETURN_PENDING:
            return to == PS_IN_SERVICE || to == PS_ON_HOLD || to == PS_MISSING || to == PS_RETURN_PENDING;
        case PS_ON_HOLD:
            return to == PS_READY_COLLECTION || to == PS_AWAITING_DELIVERY || to == PS_SPECIAL_HANDLING ||
                   to == PS_RETURN_PENDING || to == PS_MISSING;
        case PS_IN_SERVICE:
            return to == PS_COLLECTED || to == PS_DELIVERED || to == PS_RETURNED ||
                   to == PS_READY_COLLECTION || to == PS_AWAITING_DELIVERY || to == PS_SPECIAL_HANDLING ||
                   to == PS_RETURN_PENDING || to == PS_ON_HOLD || to == PS_MISSING;
        case PS_MISSING:
            return to == PS_ON_HOLD;
        default:
            return false;   // terminal states never change
    }
}

// ======================= SERVICE REQUESTS =======================
enum ServiceType {
    SVC_COLLECTION = 0, SVC_CAMPUS_DELIVERY, SVC_SPECIAL_HANDLING, SVC_REDELIVERY, SVC_RETURN, SVC_COUNT
};
enum Urgency       { URG_URGENT = 1, URG_NORMAL = 2, URG_LOW = 3 };   // lower number = more urgent
enum RequestStatus { RS_PENDING = 0, RS_IN_PROGRESS, RS_COMPLETED, RS_CANCELLED, RS_COUNT };

struct ServiceRequest {
    char id[ID_LEN];             // e.g. "REQ001"
    char parcelId[ID_LEN];
    int  serviceType;
    int  urgency;
    long sequence;               // creation order: breaks ties between equal urgency
    char created[TIME_LEN];
    int  status;                 // RequestStatus
    char resourceId[ID_LEN];     // assigned counter/courier ("" when none)
    int  parcelStatusBefore;     // parcel status before service began (restored on cancel/undo)
    char note[NOTE_LEN];         // recipient/service information, later the outcome
};

inline const char* serviceTypeName(int t) {
    static const char* n[SVC_COUNT] = { "Recipient Collection", "Campus Delivery", "Special Handling",
                                        "Re-delivery", "Return to Sender" };
    return (t >= 0 && t < SVC_COUNT) ? n[t] : "Unknown";
}
inline const char* urgencyName(int u) {
    return u == URG_URGENT ? "Urgent" : (u == URG_NORMAL ? "Normal" : (u == URG_LOW ? "Low" : "?"));
}
inline const char* requestStatusName(int s) {
    static const char* n[RS_COUNT] = { "Pending", "In Progress", "Completed", "Cancelled" };
    return (s >= 0 && s < RS_COUNT) ? n[s] : "Unknown";
}

// Which parcel states each service may start from (prevents invalid service operations).
inline bool serviceAllowedForStatus(int svc, int ps) {
    switch (svc) {
        case SVC_COLLECTION:       return ps == PS_READY_COLLECTION;
        case SVC_CAMPUS_DELIVERY:  return ps == PS_AWAITING_DELIVERY;
        case SVC_SPECIAL_HANDLING: return ps == PS_SPECIAL_HANDLING;
        case SVC_REDELIVERY:       return ps == PS_AWAITING_DELIVERY || ps == PS_READY_COLLECTION;
        case SVC_RETURN:           return canRequestService(ps);
    }
    return false;
}
// Final parcel status when a service of this type completes.
inline int serviceOutcomeStatus(int svc) {
    switch (svc) {
        case SVC_COLLECTION:
        case SVC_SPECIAL_HANDLING:  return PS_COLLECTED;
        case SVC_CAMPUS_DELIVERY:
        case SVC_REDELIVERY:        return PS_DELIVERED;
        default:                    return PS_RETURNED;
    }
}

// ============================ RESOURCES ============================
enum ResourceType   { RT_COUNTER = 0, RT_COURIER, RT_COUNT };
enum ResourceStatus { RES_AVAILABLE = 0, RES_OCCUPIED, RES_UNAVAILABLE, RES_STATUS_COUNT };

struct Resource {
    char id[ID_LEN];                // "CTR001" / "DEL001"
    char name[NAME_LEN];
    int  type;                      // ResourceType
    int  status;                    // ResourceStatus
    char assignedParcelId[ID_LEN];  // "" when idle
    char assignedRequestId[ID_LEN];
};

inline const char* resourceTypeName(int t) {
    return t == RT_COUNTER ? "Collection Counter" : (t == RT_COURIER ? "Delivery Courier" : "Unknown");
}
inline const char* resourceStatusName(int s) {
    static const char* n[RES_STATUS_COUNT] = { "Available", "Occupied", "Unavailable" };
    return (s >= 0 && s < RES_STATUS_COUNT) ? n[s] : "Unknown";
}
// Collection-style services use counters; delivery-style services use couriers.
inline int serviceResourceType(int svc) {
    return (svc == SVC_COLLECTION || svc == SVC_SPECIAL_HANDLING) ? RT_COUNTER : RT_COURIER;
}

// ============================ ACTIVITIES ============================
enum ActivityType {
    AT_PARCEL_REGISTERED = 0, AT_PARCEL_STATUS, AT_PARCEL_UPDATED, AT_PARCEL_REMOVED,
    AT_REQUEST_CREATED, AT_REQUEST_STARTED, AT_REQUEST_COMPLETED, AT_REQUEST_CANCELLED,
    AT_RESOURCE_CHANGED, AT_REVERSAL, AT_TYPE_COUNT
};

struct Activity {
    long seq;                   // global sequence number (processing order)
    char timestamp[TIME_LEN];
    int  type;                  // ActivityType
    char parcelId[ID_LEN];
    char requestId[ID_LEN];
    char resourceId[ID_LEN];
    int  oldStatus;             // parcel status before (-1 = none)
    int  newStatus;             // parcel status after  (-1 = none)
    char description[DESC_LEN];
    bool reversible;            // can this kind of activity ever be undone?
    bool reversed;              // has it already been undone?
};

inline const char* activityTypeName(int t) {
    static const char* n[AT_TYPE_COUNT] = {
        "Parcel Registered", "Status Changed", "Parcel Updated", "Parcel Removed", "Request Created",
        "Request Started", "Request Completed", "Request Cancelled", "Resource Changed", "Reversal"
    };
    return (t >= 0 && t < AT_TYPE_COUNT) ? n[t] : "Unknown";
}

#endif
