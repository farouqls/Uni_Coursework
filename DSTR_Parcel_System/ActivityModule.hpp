// ActivityModule.hpp  -- TASK 4: Parcel Activity and Record Management
#ifndef ACTIVITYMODULE_HPP
#define ACTIVITYMODULE_HPP

#include "System.hpp"

// Called by every other module whenever something important happens.
void logActivity(SystemContext& ctx, int type, const char* parcelId, const char* requestId,
                 const char* resourceId, int oldStatus, int newStatus,
                 const char* description, bool reversible);

void showRecentActivities(SystemContext& ctx, int howMany);
bool reverseLatestActivity(SystemContext& ctx);   // undo the newest reversible activity in the system
void activityMenu(SystemContext& ctx);

#endif
