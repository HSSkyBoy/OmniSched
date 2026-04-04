#include "config.h"
#include "project_paths.h"
#include "zygisk/api.hpp"

#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>

using json = nlohmann::json;

namespace {

std::string jstring_to_string(JNIEnv* env, jstring value) {
    if (env == nullptr || value == nullptr) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) return {};

    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

struct RenderConfigSnapshot {
    VulkanMode vulkan_mode = VulkanMode::OFF;
    std::unordered_set<std::string> vulkan_apps;
};

RenderConfigSnapshot load_render_config() {
    RenderConfigSnapshot snapshot;

    std::ifstream file;
    for (const auto& config_path : omnisched::config_path_candidates()) {
        file.open(config_path);
        if (file.is_open()) break;
        file.clear();
    }
    if (!file.is_open()) return snapshot;

    json data = json::parse(file, nullptr, false);
    if (data.is_discarded() || !data.contains("render") || !data["render"].is_object()) {
        return snapshot;
    }

    const auto& render = data["render"];
    const std::string mode = render.value("vulkan_mode", "off");
    if (mode == "global") snapshot.vulkan_mode = VulkanMode::GLOBAL;
    else if (mode == "per_app") snapshot.vulkan_mode = VulkanMode::PER_APP;

    if (render.contains("vulkan_apps") && render["vulkan_apps"].is_array()) {
        for (const auto& app : render["vulkan_apps"]) {
            if (app.is_string()) snapshot.vulkan_apps.emplace(app.get<std::string>());
        }
    }

    return snapshot;
}

void apply_per_app_vulkan_env() {
    // Keep mutations process-local. Real property interception will be added on top of this module.
    setenv("debug.hwui.renderer", "skiavk", 1);
    setenv("debug.renderengine.backend", "skiavk", 1);
    setenv("ro.hwui.use_vulkan", "true", 1);
    setenv("debug.renderengine.graphite", "false", 1);
    setenv("OMNISCHED_VULKAN_INJECTED", "1", 1);
}

class OmniSchedZygiskModule final : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        api_ = api;
        env_ = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        if (api_ == nullptr || env_ == nullptr || args == nullptr) return;

        api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);

        const auto snapshot = load_render_config();
        if (snapshot.vulkan_mode != VulkanMode::PER_APP || snapshot.vulkan_apps.empty()) return;

        const std::string process_name = jstring_to_string(env_, args->nice_name);
        if (!process_name.empty() && snapshot.vulkan_apps.contains(process_name)) {
            apply_per_app_vulkan_env();
        }
    }

private:
    zygisk::Api* api_ = nullptr;
    JNIEnv* env_ = nullptr;
};

}  // namespace

REGISTER_ZYGISK_MODULE(OmniSchedZygiskModule)
