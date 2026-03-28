#pragma once

#include <string>
#include <memory>

enum class RootType {
    MAGISK,
    KERNELSU,
    APATCH,
    UNKNOWN
};

class IRootAdapter {
public:
    virtual ~IRootAdapter() = default;
    virtual RootType get_type() const = 0;
    virtual std::string get_name() const = 0;
    virtual bool set_system_prop(const std::string& key, const std::string& value) const = 0;
    virtual bool inject_sched_rule(const std::string& rule) const = 0;
};

class RootEnvironment {
public:
    static const IRootAdapter& get_adapter();

private:
    static std::unique_ptr<IRootAdapter> detect_environment();
};
