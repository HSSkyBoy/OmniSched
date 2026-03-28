#include "root_adapter.h"
#include "utils.h"
#include <unistd.h>

class MagiskAdapter : public IRootAdapter {
public:
    RootType get_type() const override { return RootType::MAGISK; }
    std::string get_name() const override { return "Magisk"; }

    bool set_system_prop(const std::string& key, const std::string& value) const override {
        std::string cmd = "resetprop -n " + key + " " + value;
        execute_command(cmd.c_str());
        return true;
    }

    bool inject_sched_rule(const std::string& rule) const override {
        return true;
    }
};

class KernelSuAdapter : public IRootAdapter {
public:
    RootType get_type() const override { return RootType::KERNELSU; }
    std::string get_name() const override { return "KernelSU"; }

    bool set_system_prop(const std::string& key, const std::string& value) const override {
        std::string cmd = "setprop " + key + " " + value;
        execute_command(cmd.c_str());
        return true;
    }

    bool inject_sched_rule(const std::string& rule) const override {
        return true;
    }
};

class APatchAdapter : public IRootAdapter {
public:
    RootType get_type() const override { return RootType::APATCH; }
    std::string get_name() const override { return "APatch"; }

    bool set_system_prop(const std::string& key, const std::string& value) const override {
        std::string cmd = "setprop " + key + " " + value;
        execute_command(cmd.c_str());
        return true;
    }

    bool inject_sched_rule(const std::string& rule) const override {
        return true;
    }
};

class UnknownRootAdapter : public IRootAdapter {
public:
    RootType get_type() const override { return RootType::UNKNOWN; }
    std::string get_name() const override { return "Unknown/Standalone"; }

    bool set_system_prop(const std::string& key, const std::string& value) const override { return false; }
    bool inject_sched_rule(const std::string& rule) const override { return false; }
};


std::unique_ptr<IRootAdapter> RootEnvironment::detect_environment() {
    if (path_exists("/data/adb/ksu")) {
        return std::make_unique<KernelSuAdapter>();
    } 
    else if (path_exists("/data/adb/ap") || path_exists("/data/adb/apatch")) {
        return std::make_unique<APatchAdapter>();
    } 
    else if (path_exists("/data/adb/magisk") || path_exists("/sbin/magisk")) {
        return std::make_unique<MagiskAdapter>();
    }
    
    return std::make_unique<UnknownRootAdapter>();
}

const IRootAdapter& RootEnvironment::get_adapter() {
    static const std::unique_ptr<IRootAdapter> instance = detect_environment();
    return *instance;
}
