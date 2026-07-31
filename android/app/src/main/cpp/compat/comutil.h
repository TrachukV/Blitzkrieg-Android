#pragma once
// MSVC's COM utility header. The engine's property system stores values in
// _variant_t and names them with _bstr_t; a handful of files reach for those
// two types and nothing else in COM. These stand in for them off Windows.
#include <string>

class _bstr_t {
public:
    _bstr_t() = default;
    _bstr_t(const char* value) : value_(value ? value : "") {}
    const char* operator*() const { return value_.c_str(); }
    operator const char*() const { return value_.c_str(); }

private:
    std::string value_;
};

class _variant_t {
public:
    _variant_t() = default;
    _variant_t(int value) : int_(value) {}
    _variant_t(double value) : double_(value) {}
    _variant_t(const char* value) : string_(value ? value : "") {}

    operator int() const { return int_; }
    operator double() const { return double_; }

private:
    int int_ = 0;
    double double_ = 0.0;
    std::string string_;
};

typedef _variant_t variant_t;
typedef _bstr_t bstr_t;
