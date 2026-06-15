#pragma once

#include "metaagent.h"

#include <mutex>

namespace metaagent::app_host {

struct HostConfig {
	bool enable_ai = true;
	core::String ollama_url = "http://127.0.0.1:11434";
	core::String ollama_model = "llama3.2";
	core::String system_prompt =
		"You are a concise assistant embedded in the MetaAgent desktop application.";
	core::String platform_base_url;
	core::String platform_event_endpoint = "/api/unreal/event";
};

class MetaAgentHost {
public:
	void configure(const HostConfig& config);
	void initialize();

	void tick(float delta_seconds);

	session::RuntimeSession& session();
	const session::RuntimeSession& session() const;

	net::HandlerContext make_handler_context();
	net::RouteDispatchResult dispatch_route(const net::HttpRequest& request);

	core::String build_status_json() const;
	core::String build_gui_catalog_json() const;
	core::String dispatch_command(const core::String& command_name);
	core::String build_config_json() const;

	void on_notify(const core::String& message);
	core::String build_notify_log_json() const;

private:
	void wire_callbacks();
	void seed_mock_particles();
	bool build_pattern_targets();
	bool read_displayed_positions(particle::DisplayedPose& out_pose);
	void apply_world_positions(const core::Array<core::Vec3>& positions);
	int32_t authoritative_particle_count() const;
	void apply_command_side_effects(app::CommandId command);

	HostConfig config_;
	session::RuntimeSession session_;
	particle::ParticleScheduler scheduler_;
	particle::SchedulerCallbacks scheduler_callbacks_;
	runtime::HostServiceCallbacks host_services_;
	ai::LanguageAiRuntime language_ai_;
	ai::LanguageAiTransportCallbacks language_ai_transport_;
	net::RouteTable routes_;

	core::Array<core::Vec3> mock_world_positions_;
	core::Array<core::String> notify_log_;
	bool recording_active_ = false;
	bool autopilot_enabled_ = false;
	bool cinematic_enabled_ = false;
	bool focus_particles_ = false;
	mutable std::mutex mutex_;
};

} // namespace metaagent::app_host
