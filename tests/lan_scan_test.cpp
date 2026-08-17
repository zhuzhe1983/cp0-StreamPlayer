#include "lan_scan.h"

#include <cassert>
#include <iostream>

int main()
{
    assert(lan_normalize_base_url(" 192.168.1.10:8096/ ") == "http://192.168.1.10:8096");
    assert(lan_normalize_base_url("http://media.local:8096/") == "http://media.local:8096");

    LanServer jellyfin;
    assert(lan_parse_discovery_payload(
        "{\"Address\":\"http://192.168.1.10:8096\",\"Id\":\"abc\",\"Name\":\"Office\"}",
        "192.168.1.10", jellyfin, LanServerType::Jellyfin));
    assert(jellyfin.type == LanServerType::Jellyfin);
    assert(jellyfin.address == "http://192.168.1.10:8096");
    assert(jellyfin.name == "Office");
    assert(jellyfin.id == "abc");
    assert(lan_server_label(jellyfin) == "Jellyfin  Office");

    LanServer emby;
    assert(lan_parse_discovery_payload(
        "{\"Address\":\"http://192.168.1.12:8096\",\"Id\":\"xyz\",\"Name\":\"Emby NAS\"}",
        "192.168.1.12", emby, LanServerType::Emby));
    assert(emby.type == LanServerType::Emby);
    assert(emby.name == "Emby NAS");

    LanServer public_info;
    assert(lan_parse_public_info(
        "{\"LocalAddress\":\"http://192.168.1.10:8096\",\"ServerName\":\"Office\","
        "\"ProductName\":\"Jellyfin Server\",\"Id\":\"abc\"}",
        "http://192.168.1.10:8096", public_info));
    assert(public_info.type == LanServerType::Jellyfin);
    assert(public_info.name == "Office");

    LanServer emby_public;
    assert(lan_parse_public_info(
        "{\"LocalAddress\":\"http://192.168.1.12:8096\",\"ServerName\":\"Living Room\","
        "\"Id\":\"xyz\"}",
        "http://192.168.1.12:8096", emby_public));
    assert(emby_public.name == "Living Room");

    std::vector<LanServer> servers;
    servers.push_back(jellyfin);
    servers.push_back(public_info);
    servers.push_back(emby);
    lan_dedupe_servers(servers);
    assert(servers.size() == 2);

    std::cout << "lan_scan_test: ok\n";
    return 0;
}
