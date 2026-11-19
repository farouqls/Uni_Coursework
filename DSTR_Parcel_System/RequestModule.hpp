// RequestModule.hpp  -- TASK 2: Parcel Service and Collection Management
#ifndef REQUESTMODULE_HPP
#define REQUESTMODULE_HPP

#include <cstddef>
#include "System.hpp"

bool parcelHasActiveRequest(SystemContext& ctx, const char* parcelId);

bool createRequestRecord(SystemContext& ctx, const char* parcelId, int serviceType, int urgency,
                         const char* note, char* err, size_t errSize);
// reqId == nullptr/"" -> take the best request whose resource type is free.
bool startRequest(SystemContext& ctx, const char* reqId, const char* resId);
bool completeRequestRecord(SystemContext& ctx, const char* reqId, char* err, size_t errSize);
bool cancelRequestRecord(SystemContext& ctx, const char* reqId, char* err, size_t errSize);

void displayPendingRequests(SystemContext& ctx);
void requestMenu(SystemContext& ctx);

#endif
