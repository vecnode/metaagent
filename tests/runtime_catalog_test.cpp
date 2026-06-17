#include "app/runtime_catalog.hpp"
#include "core/types.hpp"
#include "session/types.hpp"

#include <cassert>
#include <iostream>

int main()
{
	metaagent::session::RuntimeSession runtime_session;
	runtime_session.features.networking = true;
	runtime_session.features.ai = true;
	runtime_session.features.recording = true;
	runtime_session.features.particle = true;

	const metaagent::core::Array<metaagent::app::RuntimeDescriptor> catalog_off =
		metaagent::app::build_runtime_catalog(runtime_session, false);
	assert(catalog_off.size() >= 8);

	bool found_networking = false;
	bool found_particle = false;
	bool found_recording = false;
	for (const metaagent::app::RuntimeDescriptor& descriptor : catalog_off)
	{
		if (descriptor.id == "networking")
		{
			found_networking = true;
			assert(descriptor.active_in_session);
			assert(descriptor.host_scope == "core");
		}
		if (descriptor.id == "particle")
		{
			found_particle = true;
			assert(descriptor.host_scope == "ue5");
			assert(!descriptor.active_in_session);
		}
		if (descriptor.id == "recording")
		{
			found_recording = true;
			assert(descriptor.host_scope == "ue5");
			assert(!descriptor.active_in_session);
		}
	}

	assert(found_networking);
	assert(found_particle);
	assert(found_recording);

	const metaagent::core::Array<metaagent::app::RuntimeDescriptor> catalog_on =
		metaagent::app::build_runtime_catalog(runtime_session, true);
	for (const metaagent::app::RuntimeDescriptor& descriptor : catalog_on)
	{
		if (descriptor.id == "particle")
		{
			assert(descriptor.active_in_session);
		}
	}

	const metaagent::core::String json =
		metaagent::app::build_runtime_catalog_json(runtime_session, false);
	assert(json.find("\"ue5_runtimes_enabled\":false") != metaagent::core::String::npos);
	assert(json.find("\"runtimes\"") != metaagent::core::String::npos);

	std::cout << "runtime_catalog_test passed" << std::endl;
	return 0;
}
