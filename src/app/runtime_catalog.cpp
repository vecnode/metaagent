#include "app/runtime_catalog.hpp"

#include "net/json.hpp"

#include <sstream>

namespace metaagent::app {
namespace {

RuntimeDescriptor make_runtime(
	const char* id,
	const char* title,
	const char* summary,
	const char* host_scope,
	const bool active)
{
	RuntimeDescriptor descriptor;
	descriptor.id = id;
	descriptor.title = title;
	descriptor.summary = summary;
	descriptor.host_scope = host_scope;
	descriptor.active_in_session = active;
	return descriptor;
}

} // namespace

core::Array<RuntimeDescriptor> build_runtime_catalog(const session::RuntimeSession& session)
{
	core::Array<RuntimeDescriptor> catalog;
	catalog.push_back(make_runtime(
		"networking",
		"Networking Engine",
		"Signal bus, target registry, HTTP inbound/outbound, platform event forwarding.",
		"all hosts",
		session.features.networking && session.http_enabled));
	catalog.push_back(make_runtime(
		"media",
		"Media Runtime",
		"PNG/JPEG decode, mask pipeline, tile sequencer for outbound image regions.",
		"all hosts",
		session.features.particle));
	catalog.push_back(make_runtime(
		"particle",
		"Particle Runtime",
		"Pattern FSM, scheduler, actuation, shape/mask formation, visual continuity.",
		"all hosts",
		session.features.particle));
	catalog.push_back(make_runtime(
		"camera",
		"Camera Runtime",
		"Cinematic orbit rig, zoom, focus styles (OscillatingHold / SlowOrbit).",
		"unreal host",
		session.features.camera));
	catalog.push_back(make_runtime(
		"ai",
		"Language AI Runtime",
		"Ollama chat integration, transcript, optional autopilot planning.",
		"all hosts",
		session.features.ai));
	catalog.push_back(make_runtime(
		"session",
		"Session + Commands",
		"RuntimeSession snapshot, command parse/validate, GUI action catalog.",
		"all hosts",
		session.active));
	catalog.push_back(make_runtime(
		"notify",
		"Notify Ingest",
		"Inbound /notify events, parse JSON/text, append to comms log.",
		"all hosts",
		session.http_enabled));
	catalog.push_back(make_runtime(
		"recording",
		"Recording Runtime",
		"MovieSceneCapture video capture toggles and status queries.",
		"unreal host",
		session.features.recording));
	catalog.push_back(make_runtime(
		"autopilot",
		"Autopilot Runtime",
		"AI-driven pawn autopilot toggles and status (Unreal gameplay host).",
		"unreal host",
		session.features.ai));
	catalog.push_back(make_runtime(
		"input",
		"Input Policy",
		"GUI-open vs observation-mode input routing and wheel zoom policy.",
		"unreal host",
		session.features.input));
	return catalog;
}

core::String build_runtime_catalog_json(const session::RuntimeSession& session)
{
	const core::Array<RuntimeDescriptor> catalog = build_runtime_catalog(session);
	std::ostringstream stream;
	stream << "{\"runtimes\":[";
	for (size_t index = 0; index < catalog.size(); ++index)
	{
		if (index > 0)
		{
			stream << ',';
		}
		const RuntimeDescriptor& runtime = catalog[index];
		stream << '{';
		stream << net::json_string_field("id", runtime.id) << ',';
		stream << net::json_string_field("title", runtime.title) << ',';
		stream << net::json_string_field("summary", runtime.summary) << ',';
		stream << net::json_string_field("host_scope", runtime.host_scope) << ',';
		stream << net::json_bool_field("active_in_session", runtime.active_in_session);
		stream << '}';
	}
	stream << "]}";
	return stream.str();
}

} // namespace metaagent::app
