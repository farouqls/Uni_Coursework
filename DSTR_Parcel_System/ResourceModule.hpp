// ResourceModule.hpp  -- TASK 3: Collection Counter and Delivery Resource Management
#ifndef RESOURCEMODULE_HPP
#define RESOURCEMODULE_HPP

#include "System.hpp"

bool addResource(SystemContext& ctx, int type, const char* name, bool logIt);
bool resourceTypeAvailable(SystemContext& ctx, int resType);
// Takes the next resource in rotation (or the named one) and marks it occupied.
bool assignResource(SystemContext& ctx, int resType, const char* parcelId, const char* requestId,
                    const char* specificResId, char* outResId);
bool releaseResource(SystemContext& ctx, const char* resourceId);
void displayResources(SystemContext& ctx);
void resourceMenu(SystemContext& ctx);

#endif
