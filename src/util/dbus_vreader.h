#ifndef DBUS_VREADER_H
#define DBUS_VREADER_H

#include <iostream>
#include <map>
#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <vector>

static std::string ReadDbusVariant(const sdbus::Variant& var);

inline std::string ReadDbusContainer(const std::vector<sdbus::Variant>& vec) {
    std::string res = "[";
    bool first = true;
    for (const auto& elem : vec) {
        if (!first)
            res += ", ";
        res += ReadDbusVariant(elem);
        first = false;
    }
    res += "]";
    return res;
}

inline std::string ReadDbusMap(const std::map<std::string, sdbus::Variant>& m) {
    std::string res = "{";
    bool first = true;
    for (const auto& [key, val] : m) {
        if (!first)
            res += ", ";
        res += key + ": " + ReadDbusVariant(val);
        first = false;
    }
    res += "}";
    return res;
}

static std::string ReadDbusVariant(const sdbus::Variant& var) {
    std::string sig = var.peekValueType();

    if (sig.empty())
        return "<empty>";

    char type = sig[0];

    switch (type) {
    case 'b':
        return var.get<bool>() ? "true" : "false";
    case 'y':
        return std::to_string(var.get<uint8_t>());
    case 'n':
        return std::to_string(var.get<int16_t>());
    case 'q':
        return std::to_string(var.get<uint16_t>());
    case 'i':
        return std::to_string(var.get<int32_t>());
    case 'u':
        return std::to_string(var.get<uint32_t>());
    case 'x':
        return std::to_string(var.get<int64_t>());
    case 't':
        return std::to_string(var.get<uint64_t>());
    case 'd':
        return std::to_string(var.get<double>());
    case 's':
        return "\"" + var.get<std::string>() + "\"";
    case 'o':
        return "object-path:\"" + var.get<std::string>() + "\"";
    case 'g':
        return "signature:\"" + var.get<std::string>() + "\"";
    case 'v':
        // Variant containing a variant, unwrap recursively
        return ReadDbusVariant(var.get<sdbus::Variant>());
    case 'a': {
        // array: check the second char to distinguish types
        if (sig.length() > 1 && sig[1] == '{') {
            // dictionary a{kv} usually a{sv}
            try {
                auto m = var.get<std::map<std::string, sdbus::Variant>>();
                return ReadDbusMap(m);
            } catch (...) {
                return "<invalid dictionary>";
            }
        } else {
            // array of something else (often vector<Variant>)
            try {
                auto vec = var.get<std::vector<sdbus::Variant>>();
                return ReadDbusContainer(vec);
            } catch (...) {
                try {
                    // fallback to array of strings for example
                    auto vecStr = var.get<std::vector<std::string>>();
                    std::string res = "[";
                    bool first = true;
                    for (const auto& s : vecStr) {
                        if (!first)
                            res += ", ";
                        res += "\"" + s + "\"";
                        first = false;
                    }
                    res += "]";
                    return res;
                } catch (...) {
                    return "<invalid array>";
                }
            }
        }
    }
    case '(': // struct
        return "<struct>";
    default:
        return "<unsupported type>";
    }
}

#endif // DBUS_VREADER_H
