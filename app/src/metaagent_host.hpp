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
	core::String default_target_id = "platform-default";
	core::String extra_targets;
	core::String sequence_target_id = "media-player-cpp";
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
	core::String build_network_status_json() const;
	core::String build_runtime_catalog_json() const;
	core::String build_gui_catalog_json() const;
	core::String dispatch_command(const core::String& command_name);
	core::String build_config_json() const;

	void on_notify(const core::String& message);
	core::String build_notify_log_json() const;

	core::String build_targets_json() const;
	core::String register_target(const core::String& body);
	core::String unregister_target(const core::String& target_id);
	core::String dispatch_signal(const core::String& body);
	core::String build_signal_log_json() const;

	core::String build_sequence_status_json() const;
	core::String sequence_load(const core::String& body);
	core::String sequence_control(const core::String& action);

private:
	void wire_callbacks();
	void seed_mock_particles();
	void seed_default_targets();
	bool build_pattern_targets();
	bool read_displayed_positions(particle::DisplayedPose& out_pose);
	void apply_world_positions(const core::Array<core::Vec3>& positions);
	int32_t authoritative_particle_count() const;
	void apply_command_side_effects(app::CommandId command);

	net::SignalDispatchResult send_signal(const net::SignalEnvelope& envelope);
	void log_signal_delivery(
		const core::String& direction,
		const net::SignalEnvelope& envelope,
		const net::SignalDispatchResult& result,
		const core::String& summary);
	net::SignalTransportFn make_signal_transport() const;
	void tick_sequence(float delta_seconds);
	bool emit_current_tile_signal();

	HostConfig config_;
	session::RuntimeSession session_;
	particle::ParticleScheduler scheduler_;
	particle::SchedulerCallbacks scheduler_callbacks_;
	runtime::HostServiceCallbacks host_services_;
	ai::LanguageAiRuntime language_ai_;
	ai::LanguageAiTransportCallbacks language_ai_transport_;
	net::RouteTable routes_;
	net::SignalRouter signal_router_;
	media::TileSequencer tile_sequencer_;

	core::Array<core::Vec3> mock_world_positions_;
	core::Array<core::String> notify_log_;
	bool recording_active_ = false;
	bool autopilot_enabled_ = false;
	bool cinematic_enabled_ = false;
	bool focus_particles_ = false;
	int32_t last_sequence_tile_sent_ = -1;
	mutable std::mutex mutex_;
};

} // namespace metaagent::app_host
