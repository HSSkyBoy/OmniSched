#include "config.h"
#include "project_paths.h"
#include "zygisk/api.hpp"

#include <sys/stat.h>
#include <sys/system_properties.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_set>
#include <utility>
#include <nlohmann/json.hpp>

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

    const char* get_vulkan_prop_override(const std::string& prop_name) {
        static const std::pair<const char*, const char*> overrides[] = {
            {"ro.hwui.renderer", "skiavk"},
            {"debug.hwui.renderer", "skiavk"},
            {"debug.renderengine.backend", "skiavk"},
            {"ro.hwui.use_vulkan", "true"},
            {"debug.renderengine.graphite", "false"},
            {"debug.renderengine.vulkan", "true"},
            {"debug.renderengine.vulkan.precompile.enabled", "true"},
            {"debug.hwui.vulkan_feature_level", "1.3"},
            {"debug.hwui.vulkan.auto_detect_features", "true"},
            {"debug.hwui.vulkan.platform_optimized", "true"},
            {"debug.hwui.vulkan.enable_dynamic_rendering", "true"},
            {"debug.hwui.vulkan.synchronization2", "true"},
            {"debug.hwui.vulkan.enable_descriptor_indexing", "true"},
            {"debug.hwui.vulkan.host_image_copy", "true"},
            {"debug.hwui.vulkan.dynamic_rendering_local_read", "true"},
            {"debug.hwui.vulkan.descriptor_heap", "true"},
            {"debug.hwui.vulkan.fragment_shading_rate", "true"},
            {"debug.hwui.vulkan.maintenance6", "true"},
            {"debug.hwui.vulkan.pipeline_robustness", "true"},
            {"debug.hwui.vulkan.pipeline_cache_persistent", "true"},
            {"debug.hwui.enable_gpu_pipeline_cache", "true"},
            {"debug.hwui.precompile_shaders", "true"},
            {"debug.hwui.shader_cache_preload", "true"},
            {"debug.hwui.shader_cache_warmup", "true"},
            {"debug.hwui.enable_compute_shaders", "true"},
            {"debug.vulkan.memory.preallocate", "true"},
            {"debug.vulkan.memory.sub_allocation", "true"},
            {"debug.hwui.fallback_renderer", "skiagl"},
            {"debug.hwui.initialize_gl_always", "false"},
            {"debug.hwui.early_preload_gl_context", "false"},
            {"debug.hwui.vulkan_safe_mode", "false"},
            {"debug.hwui.skia_tracing_enabled", "false"},
            {"debug.hwui.skia_use_perfetto_track_events", "false"},
            {"debug.renderengine.skia_atrace_enabled", "false"},
            {"debug.vulkan.force_validation", "false"},
            {"debug.vulkan.validate.memory", "false"},
            {"debug.vulkan.validate", "false"},
            {"debug.hwui.use_hint_manager", "true"},
            {"debug.sf.enable_async_barrier_control", "true"}
        };

        for (const auto& [key, value] : overrides) {
            if (prop_name == key) return value;
        }
        return nullptr;
    }

    RenderConfigSnapshot load_render_config() {
        static RenderConfigSnapshot cached_snapshot;
        static time_t last_modified_time = 0;
        static std::mutex cache_mutex;

        std::lock_guard<std::mutex> lock(cache_mutex);

        std::string active_path;
        struct stat st;
        bool file_found = false;

        for (const auto& config_path : omnisched::config_path_candidates()) {
            if (stat(config_path.c_str(), &st) == 0) {
                active_path = config_path;
                file_found = true;
                break;
            }
        }

        if (!file_found) return cached_snapshot;

        if (st.st_mtime == last_modified_time && last_modified_time != 0) {
            return cached_snapshot;
        }

        std::ifstream file(active_path);
        if (!file.is_open()) return cached_snapshot;

        json data = json::parse(file, nullptr, false);
        if (data.is_discarded() || !data.contains("render") || !data["render"].is_object()) {
            return cached_snapshot;
        }

        RenderConfigSnapshot new_snapshot;
        const auto& render = data["render"];
        const std::string mode = render.value("vulkan_mode", "off");

        if (mode == "global") new_snapshot.vulkan_mode = VulkanMode::GLOBAL;
        else if (mode == "per_app") new_snapshot.vulkan_mode = VulkanMode::PER_APP;

        if (render.contains("vulkan_apps") && render["vulkan_apps"].is_array()) {
            for (const auto& app : render["vulkan_apps"]) {
                if (app.is_string()) new_snapshot.vulkan_apps.emplace(app.get<std::string>());
            }
        }

        cached_snapshot = std::move(new_snapshot);
        last_modified_time = st.st_mtime;

        return cached_snapshot;
    }

    static int (*old_system_property_get)(const char*, char*);

    static int new_system_property_get(const char* name, char* value) {
        if (name == nullptr || value == nullptr) {
            return old_system_property_get ? old_system_property_get(name, value) : 0;
        }

        std::string prop_name(name);
        const char* override_value = get_vulkan_prop_override(prop_name);
        if (override_value != nullptr) {
            strcpy(value, override_value);
            return strlen(value);
        }

        return old_system_property_get ? old_system_property_get(name, value) : 0;
    }

    class OmniSchedZygiskModule final : public zygisk::ModuleBase {
    public:
        void onLoad(zygisk::Api* api, JNIEnv* env) override {
            api_ = api;
            env_ = env;

            load_render_config();
        }

        void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
            if (api_ == nullptr || env_ == nullptr || args == nullptr) return;

            const auto snapshot = load_render_config();

            if (snapshot.vulkan_mode != VulkanMode::PER_APP || snapshot.vulkan_apps.empty()) {
                api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
                return;
            }

            const std::string process_name = jstring_to_string(env_, args->nice_name);

            if (!process_name.empty() && snapshot.vulkan_apps.contains(process_name)) {
                apply_per_app_vulkan_env();
            } else {
                api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            }
        }

    private:
        zygisk::Api* api_ = nullptr;
        JNIEnv* env_ = nullptr;

        void apply_per_app_vulkan_env() {
            static const std::pair<const char*, const char*> env_overrides[] = {
                {"debug.hwui.renderer", "skiavk"},
                {"debug.renderengine.backend", "skiavk"},
                {"ro.hwui.use_vulkan", "true"},
                {"debug.renderengine.graphite", "false"},
                {"debug.renderengine.vulkan", "true"},
                {"debug.renderengine.vulkan.precompile.enabled", "true"},
                {"debug.hwui.vulkan_feature_level", "1.3"},
                {"debug.hwui.vulkan.auto_detect_features", "true"},
                {"debug.hwui.vulkan.platform_optimized", "true"},
                {"debug.hwui.vulkan.enable_dynamic_rendering", "true"},
                {"debug.hwui.vulkan.synchronization2", "true"},
                {"debug.hwui.vulkan.enable_descriptor_indexing", "true"},
                {"debug.hwui.vulkan.host_image_copy", "true"},
                {"debug.hwui.vulkan.dynamic_rendering_local_read", "true"},
                {"debug.hwui.vulkan.descriptor_heap", "true"},
                {"debug.hwui.vulkan.fragment_shading_rate", "true"},
                {"debug.hwui.vulkan.maintenance6", "true"},
                {"debug.hwui.vulkan.pipeline_robustness", "true"},
                {"debug.hwui.vulkan.pipeline_cache_persistent", "true"},
                {"debug.hwui.enable_gpu_pipeline_cache", "true"},
                {"debug.hwui.precompile_shaders", "true"},
                {"debug.hwui.shader_cache_preload", "true"},
                {"debug.hwui.shader_cache_warmup", "true"},
                {"debug.hwui.enable_compute_shaders", "true"},
                {"debug.vulkan.memory.preallocate", "true"},
                {"debug.vulkan.memory.sub_allocation", "true"},
                {"debug.hwui.fallback_renderer", "skiagl"},
                {"debug.hwui.initialize_gl_always", "false"},
                {"debug.hwui.early_preload_gl_context", "false"},
                {"debug.hwui.vulkan_safe_mode", "false"},
                {"debug.hwui.skia_tracing_enabled", "false"},
                {"debug.hwui.skia_use_perfetto_track_events", "false"},
                {"debug.renderengine.skia_atrace_enabled", "false"},
                {"debug.vulkan.force_validation", "false"},
                {"debug.vulkan.validate.memory", "false"},
                {"debug.vulkan.validate", "false"},
                {"debug.hwui.use_hint_manager", "true"},
                {"debug.sf.enable_async_barrier_control", "true"}
            };

            for (const auto& [key, value] : env_overrides) {
                setenv(key, value, 1);
            }
            setenv("OMNISCHED_VULKAN_INJECTED", "1", 1);

            api_->pltHookRegister(0, 0, "__system_property_get",
                    (void*)new_system_property_get,
                    (void**)&old_system_property_get);

            api_->pltHookCommit();
        }
    };

}  // namespace

REGISTER_ZYGISK_MODULE(OmniSchedZygiskModule)
