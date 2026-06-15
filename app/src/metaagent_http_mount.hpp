#pragma once

namespace httplib {
class Server;
}

namespace metaagent::app_host {

class MetaAgentHost;

void mount_metaagent_routes(httplib::Server& server, MetaAgentHost& host);

} // namespace metaagent::app_host
