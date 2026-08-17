#pragma once

#include <string>
#include <vector>

enum class LanServerType { Jellyfin, Emby, Unknown };

struct LanServer {
    LanServerType type = LanServerType::Unknown;
    std::string name;
    std::string address;
    std::string id;
};

std::string lan_normalize_base_url(std::string url);
std::string lan_server_type_name(LanServerType type);
std::string lan_server_label(const LanServer &server);
bool lan_parse_discovery_payload(const std::string &payload,
                                 const std::string &from_ip,
                                 LanServer &out,
                                 LanServerType hint = LanServerType::Unknown);
bool lan_parse_public_info(const std::string &body,
                           const std::string &fallback_url,
                           LanServer &out);
void lan_dedupe_servers(std::vector<LanServer> &servers);
std::vector<LanServer> lan_scan_servers(int udp_timeout_ms = 1600,
                                        bool http_fallback = true);
