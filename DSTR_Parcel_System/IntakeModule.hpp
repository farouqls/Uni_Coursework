// IntakeModule.hpp  -- TASK 1: Parcel Intake, Sorting and Processing
#ifndef INTAKEMODULE_HPP
#define INTAKEMODULE_HPP

#include <cstddef>
#include "System.hpp"

// The single gate every parcel status change passes through (validates the transition).
bool changeParcelStatus(SystemContext& ctx, const char* parcelId, int newStatus,
                        const char* description, bool logIt, bool reversible);

int  suggestStatus(const Parcel& p, const char** reasonOut);
bool registerParcelRecord(SystemContext& ctx, Parcel& p, char* err, size_t errSize);
int  loadParcelsFromCsv(SystemContext& ctx, const char* filename, int* skipped);
int  autoSortAllParcels(SystemContext& ctx);
void intakeMenu(SystemContext& ctx);

#endif
