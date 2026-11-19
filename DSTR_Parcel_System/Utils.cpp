// Utils.cpp
#include <iostream>
#include <cstdlib>
#include <cctype>
#include <ctime>
#include "Utils.hpp"

using namespace std;

void copyStr(char* dst, size_t size, const char* src) {
    if (size == 0) return;
    size_t i = 0;
    for (; i + 1 < size && src[i] != '\0'; i++) dst[i] = src[i];
    dst[i] = '\0';
}

void getTimestamp(char* out, size_t size) {
    time_t now = time(nullptr);
    struct tm* local = localtime(&now);
    strftime(out, size, "%Y-%m-%d %H:%M", local);
}

void trimInPlace(char* s) {
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
    size_t start = 0;
    while (s[start] != '\0' && isspace((unsigned char)s[start])) start++;
    if (start > 0) memmove(s, s + start, strlen(s + start) + 1);
}

void normalizeId(char* s) {
    trimInPlace(s);
    for (size_t i = 0; s[i] != '\0'; i++) s[i] = (char)toupper((unsigned char)s[i]);
}

bool equalsIgnoreCase(const char* a, const char* b) {
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

bool containsIgnoreCase(const char* haystack, const char* needle) {
    size_t n = strlen(needle);
    if (n == 0) return true;
    for (size_t i = 0; haystack[i] != '\0'; i++) {
        size_t k = 0;
        while (k < n && haystack[i + k] != '\0' &&
               tolower((unsigned char)haystack[i + k]) == tolower((unsigned char)needle[k])) k++;
        if (k == n) return true;
    }
    return false;
}

bool isValidContact(const char* s) {
    size_t i = 0, digits = 0;
    if (s[0] == '+') i = 1;
    for (; s[i] != '\0'; i++) {
        if (!isdigit((unsigned char)s[i])) return false;
        digits++;
    }
    return digits >= 7 && digits <= 15;
}

void readLine(const char* prompt, char* buf, size_t size) {
    cout << prompt;
    if (!cin.getline(buf, (streamsize)size)) {
        if (cin.eof()) { cout << "\n[Input closed - exiting]\n"; exit(0); }
        cin.clear();                    // line was longer than the buffer: keep the first part
        cin.ignore(10000, '\n');
    }
    trimInPlace(buf);
}

int readInt(const char* prompt, int lo, int hi) {
    char buf[64];
    while (true) {
        readLine(prompt, buf, sizeof(buf));
        char* end = nullptr;
        long v = strtol(buf, &end, 10);
        if (buf[0] != '\0' && *end == '\0' && v >= lo && v <= hi) return (int)v;
        cout << "  Invalid input. Please enter a number from " << lo << " to " << hi << ".\n";
    }
}

bool confirm(const char* prompt) {
    char buf[16];
    readLine(prompt, buf, sizeof(buf));
    return buf[0] == 'y' || buf[0] == 'Y';
}

void printRule(int width) {
    for (int i = 0; i < width; i++) cout << '-';
    cout << "\n";
}

void printHeading(const char* title) {
    cout << "\n";
    printRule(70);
    cout << "  " << title << "\n";
    printRule(70);
}

void printPad(const char* text, int width) {
    int i = 0;
    for (; i < width && text[i] != '\0'; i++) cout << text[i];
    for (; i < width; i++) cout << ' ';
    cout << ' ';
}

void printParcelHeader() {
    printPad("ID", 8); printPad("Recipient", 20); printPad("Type", 10);
    printPad("Mode", 11); printPad("Status", 20); printPad("Arrived", 16);
    cout << "\n";
    printRule(91);
}

void printParcelRow(const Parcel& p) {
    printPad(p.id, 8); printPad(p.recipientName, 20); printPad(parcelTypeName(p.type), 10);
    printPad(deliveryModeName(p.deliveryMode), 11); printPad(parcelStatusName(p.status), 20);
    printPad(p.arrival, 16);
    cout << "\n";
}

void printParcelDetails(const Parcel& p) {
    cout << "  Parcel ID      : " << p.id << "\n";
    cout << "  Recipient      : " << p.recipientName << "\n";
    cout << "  Contact        : " << p.recipientContact << "\n";
    cout << "  Parcel type    : " << parcelTypeName(p.type) << "\n";
    cout << "  Delivery mode  : " << deliveryModeName(p.deliveryMode) << "\n";
    cout << "  Address        : " << p.deliveryAddress << "\n";
    cout << "  Arrived        : " << p.arrival << "\n";
    cout << "  Current status : " << parcelStatusName(p.status) << "\n";
}

void printRequestHeader() {
    printPad("ID", 8); printPad("Parcel", 8); printPad("Service", 20); printPad("Urgency", 8);
    printPad("Status", 12); printPad("Resource", 8); printPad("Created", 16);
    cout << "\n";
    printRule(84);
}

void printRequestRow(const ServiceRequest& r) {
    printPad(r.id, 8); printPad(r.parcelId, 8); printPad(serviceTypeName(r.serviceType), 20);
    printPad(urgencyName(r.urgency), 8); printPad(requestStatusName(r.status), 12);
    printPad(r.resourceId[0] ? r.resourceId : "-", 8); printPad(r.created, 16);
    cout << "\n";
}

void printRequestDetails(const ServiceRequest& r) {
    cout << "  Request ID     : " << r.id << "\n";
    cout << "  Parcel ID      : " << r.parcelId << "\n";
    cout << "  Service        : " << serviceTypeName(r.serviceType) << "\n";
    cout << "  Urgency        : " << urgencyName(r.urgency) << "\n";
    cout << "  Status         : " << requestStatusName(r.status) << "\n";
    cout << "  Resource       : " << (r.resourceId[0] ? r.resourceId : "-") << "\n";
    cout << "  Created        : " << r.created << "\n";
    cout << "  Notes          : " << (r.note[0] ? r.note : "-") << "\n";
}
