#include "metaagent_host.hpp"
#include "metaagent_http_mount.hpp"

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wshadow"
# pragma GCC diagnostic ignored "-Wconversion"
#endif

#include <httplib.h>
#include <cpp-embedlib-httplib.h>
#include "MetaAgentWebAssets.h"
#include "webview/webview.h"

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/post.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef METAAGENT_APP_INCLUDE_TERMINAL_ON_RELEASE
#define METAAGENT_APP_INCLUDE_TERMINAL_ON_RELEASE 0
#endif

namespace {

constexpr std::size_t kMaxRequestBodyBytes = 64 * 1024;
constexpr const char* kAppVersion = "0.1.0";

std::string env_or_default(const char* name, const char* default_value)
{
#if defined(_WIN32)
	char* buffer = nullptr;
	size_t length = 0;
	if (_dupenv_s(&buffer, &length, name) == 0 && buffer != nullptr)
	{
		const std::string value = buffer;
		free(buffer);
		if (!value.empty())
		{
			return value;
		}
	}
#else
	if (const char* value = std::getenv(name))
	{
		if (value[0] != '\0')
		{
			return value;
		}
	}
#endif
	return default_value;
}

bool env_flag_enabled(const char* name)
{
	const std::string value = env_or_default(name, "0");
	return value == "1" || value == "true" || value == "TRUE" || value == "yes" || value == "YES";
}

metaagent::app_host::HostConfig load_host_config()
{
	metaagent::app_host::HostConfig config;
	config.enable_ai = !env_flag_enabled("METAAGENT_NO_AI");
	config.ollama_url = env_or_default("METAAGENT_OLLAMA_URL", "http://127.0.0.1:11434");
	config.ollama_model = env_or_default("METAAGENT_OLLAMA_MODEL", "llama3.2");
	config.system_prompt = env_or_default(
		"METAAGENT_SYSTEM_PROMPT",
		"You are a concise assistant embedded in the MetaAgent desktop application.");
	config.platform_base_url = env_or_default("METAAGENT_PLATFORM_URL", "");
	config.platform_event_endpoint = env_or_default(
		"METAAGENT_PLATFORM_EVENT_ENDPOINT",
		"/api/unreal/event");
	return config;
}

void schedule_tick(
	boost::asio::steady_timer& timer,
	metaagent::app_host::MetaAgentHost& host,
	std::chrono::milliseconds interval)
{
	timer.expires_after(interval);
	timer.async_wait([&timer, &host, interval](const boost::system::error_code& error)
	{
		if (error)
		{
			return;
		}
		host.tick(static_cast<float>(interval.count()) / 1000.0f);
		schedule_tick(timer, host, interval);
	});
}

#ifdef _WIN32
std::filesystem::path executable_dir()
{
	wchar_t path[MAX_PATH] {};
	const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
	if (length == 0 || length >= MAX_PATH)
	{
		return std::filesystem::current_path();
	}
	return std::filesystem::path(path).parent_path();
}

void apply_windows_icons(webview::webview& window)
{
	const auto window_result = window.window();
	if (!window_result.ok())
	{
		return;
	}

	const auto hwnd = static_cast<HWND>(window_result.value());
	if (!hwnd)
	{
		return;
	}

	const auto icon_dir = executable_dir() / "icons";
	const auto small_icon_path = (icon_dir / "app_icon_small.ico").wstring();
	const auto large_icon_path = (icon_dir / "app_icon.ico").wstring();

	const auto small_icon = static_cast<HICON>(LoadImageW(
		nullptr,
		small_icon_path.c_str(),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON),
		LR_LOADFROMFILE));

	const auto large_icon = static_cast<HICON>(LoadImageW(
		nullptr,
		large_icon_path.c_str(),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXICON),
		GetSystemMetrics(SM_CYICON),
		LR_LOADFROMFILE));

	if (small_icon)
	{
		SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));
	}
	if (large_icon)
	{
		SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(large_icon));
	}
}
#endif

} // namespace

int main()
{
	std::cout << "metaagent-app v" << kAppVersion << std::endl;

	metaagent::app_host::MetaAgentHost host;
	host.configure(load_host_config());
	host.initialize();

	boost::asio::io_context io_context;
	auto work_guard = boost::asio::make_work_guard(io_context);

	boost::asio::steady_timer tick_timer {io_context};
	schedule_tick(tick_timer, host, std::chrono::milliseconds {16});

	std::thread io_thread([&io_context]() { io_context.run(); });

	httplib::Server server;
	server.set_read_timeout(std::chrono::seconds {3});
	server.set_write_timeout(std::chrono::seconds {3});
	server.set_keep_alive_max_count(1);
	server.set_payload_max_length(kMaxRequestBodyBytes);

	metaagent::app_host::mount_metaagent_routes(server, host);
	httplib::mount(server, MetaAgentWeb::FS);

	const int port = server.bind_to_any_port("127.0.0.1");
	if (port <= 0)
	{
		std::cerr << "[fatal] Failed to bind HTTP server to 127.0.0.1" << std::endl;
		return 1;
	}

	std::cout << "metaagent-app listening on http://127.0.0.1:" << port << std::endl;

	std::thread server_thread([&server]() { server.listen_after_bind(); });

	webview::webview window(false, nullptr);
	window.set_title("MetaAgent");
	window.set_size(1024, 720, WEBVIEW_HINT_NONE);
#ifdef _WIN32
	apply_windows_icons(window);
#endif
	window.navigate("http://127.0.0.1:" + std::to_string(port));
	window.run();

	server.stop();
	server_thread.join();

	work_guard.reset();
	tick_timer.cancel();
	io_context.stop();
	io_thread.join();

	return 0;
}
