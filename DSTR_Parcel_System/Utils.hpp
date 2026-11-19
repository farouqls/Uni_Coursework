// Utils.hpp
// Input validation helpers and console table printing shared by every module.
#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstddef>
#include "Types.hpp"

// ---- text helpers ----
void copyStr(char* dst, size_t size, const char* src);   // safe copy, always null-terminated
void getTimestamp(char* out, size_t size);               // "YYYY-MM-DD HH:MM"
void trimInPlace(char* s);
void normalizeId(char* s);                               // trim + upper-case (IDs are case-insensitive)
bool equalsIgnoreCase(const char* a, const char* b);
bool containsIgnoreCase(const char* haystack, const char* needle);
bool isValidContact(const char* s);                      // 7-15 digits, optional leading '+'

// ---- input ----
void readLine(const char* prompt, char* buf, size_t size);
int  readInt(const char* prompt, int lo, int hi);        // loops until valid
bool confirm(const char* prompt);                        // y/n

// ---- output ----
void printRule(int width);
void printHeading(const char* title);
void printPad(const char* text, int width);
void printParcelHeader();
void printParcelRow(const Parcel& p);
void printParcelDetails(const Parcel& p);
void printRequestHeader();
void printRequestRow(const ServiceRequest& r);
void printRequestDetails(const ServiceRequest& r);

#endif
