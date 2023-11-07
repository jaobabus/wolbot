#pragma once

/*
#ifndef NO_RTTI
#include <type_info>
template<typename T>
inline const char* raw_config_get_type_name() { return typeid(T).name(); }
#else
template<typename T>
struct raw_config_get_type_name_s { static constexpr const char data[] = { "<unknown>" }; };
template<>
struct raw_config_get_type_name_s<int8_t> { static constexpr const char data[] = { "int8_t" }; };
template<>
struct raw_config_get_type_name_s<uint8_t> { static constexpr const char data[] = { "uint8_t" }; };
template<>
struct raw_config_get_type_name_s<int16_t> { static constexpr const char data[] = { "int16_t" }; };
template<>
struct raw_config_get_type_name_s<uint16_t> { static constexpr const char data[] = { "uint16_t" }; };
template<>
struct raw_config_get_type_name_s<int32_t> { static constexpr const char data[] = { "int32_t" }; };
template<>
struct raw_config_get_type_name_s<uint32_t> { static constexpr const char data[] = { "uint32_t" }; };
template<>
struct raw_config_get_type_name_s<int64_t> { static constexpr const char data[] = { "int64_t" }; };
template<>
struct raw_config_get_type_name_s<uint64_t> { static constexpr const char data[] = { "uint64_t" }; };
template<>
struct raw_config_get_type_name_s<bool> { static constexpr const char data[] = { "bool" }; };
#endif
*/

#ifdef __GNUC__
#define RAW_CONFIG_ATTRIBUTE(attr) __attribute__(attr)
#else
#define RAW_CONFIG_ATTRIBUTE(attr)
#endif




namespace raw_config {




struct Error {
    enum CodeCathegory : uint8_t {
        Internal = 1,
        Key = 2,
        Argument = 3,
    };

    enum Code : uint8_t {
        Ok,
        TooSmallBuffer  = 0x10 | 0x01,
        InvalidKey      = 0x20 | 0x01,
        KeyNotFound     = 0x20 | 0x02,
        OutOfRange      = 0x20 | 0x03,
        InvalidArgument = 0x30 | 0x01,
    };

    static CodeCathegory get_cathegory(Code code) {
        return CodeCathegory(uint8_t(code) >> 4);
    }

    static Error make(const char* desc, Code code, uint16_t pos) {
        Error err;
        strncpy(err.description, desc, sizeof(err.description));
        err.null = 0;
        err.code = code;
        err.pos = pos;
        return err;
    }

    static Error fmake(Code code, uint16_t pos, const char* fmt, ...) {
        va_list va;
        va_start(va, fmt);
        Error err;
        auto e = vsnprintf(err.description, sizeof(err.description), fmt, va);
        err.description[e] = '\0';
        err.null = 0;
        err.code = code;
        err.pos = pos;
        return err;
    }

    static Error ok() {
        return make("", Code::Ok, 0);
    }

    char description[60];
    uint8_t null;
    Code code;
    uint16_t pos;
};

static Error::CodeCathegory get_cathegory(Error err) {
    return Error::get_cathegory(err.code);
}


template<typename T>
Error scanf_cast(void* data, size_t data_size, const char* string);
template<typename T>
Error printf_cast(char* string, size_t& size, const void* data, size_t data_size);


Error scanf_intx_cast(void* data, const char* fmt, const char* string) {
    if (sscanf(string, fmt, data) != 1)
        return Error::fmake(Error::Code::InvalidArgument, 0, "Error cast number '%s'", string);
    return Error::ok();
}
template<>
Error scanf_cast<char*>(void* data, size_t s, const char* string) {
    char fmt[8];
    auto e = snprintf(fmt, sizeof(fmt) - 1, "%%%ds", int(s - 1));
    fmt[e] = 0;
    if (sscanf(string, fmt, data) != 1)
        return Error::make("Error cast string", Error::Code::InvalidArgument, 0);
    return Error::ok();
}
template<>
Error scanf_cast<bool>(void* data, size_t, const char* string) {
    return scanf_intx_cast(data, "%hhd", string);
}
template<>
Error scanf_cast<int16_t>(void* data, size_t, const char* string) {
    return scanf_intx_cast(data, "%hd", string);
}
template<>
Error scanf_cast<uint16_t>(void* data, size_t, const char* string) {
    return scanf_intx_cast(data, "%hu", string);
}
template<>
Error scanf_cast<int32_t>(void* data, size_t, const char* string) {
    return scanf_intx_cast(data, "%d", string);
}
template<>
Error scanf_cast<uint32_t>(void* data, size_t, const char* string) {
    return scanf_intx_cast(data, "%u", string);
}
template<>
Error scanf_cast<int64_t>(void* data, size_t, const char* string) {
    return scanf_intx_cast(data, "%ld", string);
}
template<>
Error scanf_cast<uint64_t>(void* data, size_t, const char* string) {
    return scanf_intx_cast(data, "%lu", string);
}

Error printf_intx_cast(char* string, size_t& size, const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    size_t s = vsnprintf(string, size, fmt, va);
    if (s == size) {
        size = 0;
        return Error::make("Error cast", Error::Code::TooSmallBuffer, 0);
    }
    size = s;
    return Error::ok();
}
template<>
Error printf_cast<char*>(char* string, size_t& size, const void* data, size_t) {
    return printf_intx_cast(string, size, "%s", data);
}
template<>
Error printf_cast<int64_t>(char* string, size_t& size, const void* data, size_t) {
    return printf_intx_cast(string, size, "%lld", *(const int64_t*)data);
}
template<>
Error printf_cast<uint64_t>(char* string, size_t& size, const void* data, size_t) {
    return printf_intx_cast(string, size, "%llu", *(const uint64_t*)data);
}
template<>
Error printf_cast<int32_t>(char* string, size_t& size, const void* data, size_t) {
    return printf_intx_cast(string, size, "%d", *(const int32_t*)data);
}
template<>
Error printf_cast<uint32_t>(char* string, size_t& size, const void* data, size_t) {
    return printf_intx_cast(string, size, "%u", *(const uint32_t*)data);
}
template<>
Error printf_cast<int16_t>(char* string, size_t& size, const void* data, size_t) {
    return printf_intx_cast(string, size, "%hd", *(const int16_t*)data);
}
template<>
Error printf_cast<uint16_t>(char* string, size_t& size, const void* data, size_t) {
    return printf_intx_cast(string, size, "%hu", *(const uint16_t*)data);
}
template<>
Error printf_cast<bool>(char* string, size_t& size, const void* data, size_t) {
    return printf_intx_cast(string, size, "%hhd", *(const uint8_t*)data);
}


const char* strcode(Error::Code code) {
    switch (code)
    {
    case raw_config::Error::Ok:               return "ok";
    case raw_config::Error::InvalidKey:       return "invalid key";
    case raw_config::Error::KeyNotFound:      return "key not found";
    case raw_config::Error::InvalidArgument:  return "invalid argument";
    case raw_config::Error::TooSmallBuffer:   return "too small buffer";
    case raw_config::Error::OutOfRange:       return "out of range";
    default:                                  return "unknown";
    }
}



class NodeView
{
public:
    using FromStringCast = Error(void* data, size_t data_size, const char* string);
    using ToStringCast = Error(char* string, size_t& size, const void* data, size_t data_size);

public:
    constexpr NodeView(const char* key, size_t offset, size_t item_size, size_t total_size, FromStringCast* cast, ToStringCast* cast2, const char* type = "any")
        : key(key), offset(offset), item_size(item_size), total_size(total_size), from_str(cast), to_str(cast2), type(type) {}

public:
    const char* key;
    const size_t offset;
    const size_t item_size;
    const size_t total_size;
    FromStringCast* const from_str;
    ToStringCast* const to_str;
    const char* type;

};


class ConfigView
{
public:
    template<size_t s>
    ConfigView(void* data, const NodeView (&nodes)[s])
        : ConfigView(data, nodes, s) {}
    ConfigView(void* data, const NodeView* nodes, size_t count)
        : _data(data), _nodes(nodes), _count(count) {}


    Error get(char* result, size_t res_size, const char* key) {
        Error err = Error::ok();
        const NodeView* node;
        int16_t index = -1;
        err = parse_find(key, &node, &index);
        if (err.code != Error::Code::Ok)
            return err;
        if (index == -1) {
            size_t used = 0;
            res_size--;
            for (size_t i = 0; i < node->total_size / node->item_size && used < res_size; i++) {
                size_t size = res_size - used;
                Error err = node->to_str(result + used, size, (char*)_data + node->offset + node->item_size * i, node->item_size);
                used += size;
                if (err.code)
                    used += snprintf(result + used, res_size - used, "%s", err.description);
                used += snprintf(result + used, res_size - used, ",");
            }
            result[used] = '\0';
        }
        else {
            size_t size = res_size, used = 0;
            err = node->to_str(result, size, (char*)_data + node->offset + node->item_size * index, node->item_size);
            used += size;
            result[used] = '\0';
        }
        return err;
    }

    Error set(const char* key, const char* value) {
        const NodeView* node;
        int16_t index = -1;
        auto err = parse_find(key, &node, &index);
        if (err.code != Error::Code::Ok)
            return err;
        if (index == -1) {
            return node->from_str((char*)_data + node->offset, node->item_size, value);
        }
        else {
            return node->from_str((char*)_data + node->offset + node->item_size * index, node->item_size, value);
        }
        return err;
    }


private:
    Error parse_find(const char* key, const NodeView** node, int16_t* index) {
        const char* it = key;
        const char* index_start = it;
        const char* name_start = it;
        while ((*it >= 'a' && *it <= 'z') || (*it >= 'A' && *it <= 'Z')
            || (*it >= '0' && *it <= '9')
            || *it == '_' || *it == '-')
            ++it;
        const char* name_end = it;
        if (*it == '\0') {
            *index = -1;
        }
        else if (*it == '[') {
            it++;
            index_start = it;
            while (*it >= '0' && *it <= '9')
                ++it;
            if (*it != ']') {
                return Error::fmake(Error::Code::InvalidKey, uint16_t(it - key),
                                    "Unexpected symbol '%c' (%d)", *it, int(*it));
            }
            *index = atoi(index_start);
        }
        else {
            return Error::fmake(Error::Code::InvalidKey, uint16_t(it - key),
                                "Unexpected symbol '%c' (%d)", *it, int(*it));
        }
        for (size_t i = 0; i < _count; i++) {
            auto& refnode = _nodes[i];
            if (strncmp(refnode.key, name_start, name_end - name_start) == 0
                && strlen(refnode.key) == size_t(name_end - name_start)) {
                *node = &refnode;
                int range = int(refnode.total_size / refnode.item_size);
                if (range <= *index) {
                    return Error::fmake(Error::Code::OutOfRange, index_start - key,
                                        "Index %d is out of range 0..%d", *index, range);
                }
                return Error::ok();
            }
        }
        return Error::fmake(Error::Code::KeyNotFound, 0,
                            "Key '%s' not found", key);
    }


private:
    void* _data;
    const NodeView* _nodes;
    size_t _count;


};

constexpr size_t diff(const void* a, const void* b) {
    return (const char*)b - (const char*)a;
}

template<typename T, typename F_>
constexpr size_t coffsetof(F_ (T::* ptr)) {
    T a;
    return diff(&a, &(a.*ptr));
}

template<typename T, typename F_, size_t s>
constexpr size_t coffsetof(F_ (T::* ptr)[s]) {
    T a;
    return diff(&a, &(a.*ptr));
}

template<size_t size>
struct string {
    char data[size];
};

template<typename T, typename F_, size_t s>
constexpr NodeView from_pointer(const char* key, F_ (T::*ptr)[s], NodeView::FromStringCast s2i = scanf_cast<F_>,
                                NodeView::ToStringCast i2s = printf_cast<F_>, const char* type = "unknown") {
    size_t off = coffsetof(ptr);
    return NodeView(key, off, sizeof(F_), sizeof(F_) * s, s2i, i2s, type);
}

template<typename T, size_t s>
constexpr NodeView from_pointer(const char* key, char (T::*ptr)[s], NodeView::FromStringCast s2i = scanf_cast<char*>,
                                NodeView::ToStringCast i2s = printf_cast<char*>, const char* type = "unknown") {
    size_t off = coffsetof(ptr);
    return NodeView(key, off, s, s, s2i, i2s, type);
}

template<typename T, typename F_>
constexpr NodeView from_pointer(const char* key, F_ T::* ptr, NodeView::FromStringCast s2i = scanf_cast<F_>,
                                NodeView::ToStringCast i2s = printf_cast<F_>, const char* type = "unknown") {
    size_t off = coffsetof(ptr);
    return NodeView(key, off, sizeof(F_), sizeof(F_), s2i, i2s, type);
}


}

