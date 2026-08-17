#include "lan_scan.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sstream>
#include <unistd.h>
#include <vector>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>

namespace {

constexpr int kDiscoveryPort = 7359;
constexpr int kHttpPorts[] = {8096, 8097};
constexpr int kHttpWaveSize = 20;
constexpr int kHttpConnectMs = 220;
constexpr int kHttpReadMs = 380;

std::string trim_copy(std::string s)
{
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) b++;
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
    return s.substr(b, e - b);
}

std::string to_lower_copy(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

bool json_skip_ws(const std::string &s, size_t &i)
{
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
    return i < s.size();
}

std::string json_string_at(const std::string &s, size_t pos)
{
    if (!json_skip_ws(s, pos) || s[pos] != '"') return "";
    pos++;
    std::string out;
    bool escape = false;
    for (; pos < s.size(); ++pos) {
        char c = s[pos];
        if (escape) {
            switch (c) {
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            default: out += c; break;
            }
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == '"') {
            break;
        } else {
            out += c;
        }
    }
    return out;
}

size_t json_find_key(const std::string &s, const std::string &key)
{
    const std::string pat = "\"" + key + "\"";
    size_t pos = 0;
    while ((pos = s.find(pat, pos)) != std::string::npos) {
        size_t value = pos + pat.size();
        if (!json_skip_ws(s, value) || s[value] != ':') {
            pos += pat.size();
            continue;
        }
        value++;
        json_skip_ws(s, value);
        return value;
    }
    return std::string::npos;
}

std::string json_get_string(const std::string &s, const std::string &key)
{
    size_t pos = json_find_key(s, key);
    if (pos == std::string::npos) return "";
    return json_string_at(s, pos);
}

LanServerType type_from_text(const std::string &text, LanServerType fallback = LanServerType::Unknown)
{
    const std::string lower = to_lower_copy(text);
    if (lower.find("jellyfin") != std::string::npos) return LanServerType::Jellyfin;
    if (lower.find("emby") != std::string::npos) return LanServerType::Emby;
    return fallback;
}

bool looks_like_json_object(const std::string &s)
{
    size_t i = 0;
    return json_skip_ws(s, i) && s[i] == '{';
}

std::string host_from_url(const std::string &url)
{
    std::string value = url;
    const std::string scheme_end = "://";
    size_t pos = value.find(scheme_end);
    if (pos != std::string::npos) value = value.substr(pos + scheme_end.size());
    pos = value.find('/');
    if (pos != std::string::npos) value = value.substr(0, pos);
    pos = value.find('@');
    if (pos != std::string::npos) value = value.substr(pos + 1);
    if (!value.empty() && value.front() == '[') {
        size_t end = value.find(']');
        if (end != std::string::npos) return value.substr(1, end - 1);
    }
    pos = value.rfind(':');
    if (pos != std::string::npos && value.find(':') == pos) value = value.substr(0, pos);
    return value;
}

bool ipv4_from_text(const std::string &text, in_addr &out)
{
    return inet_pton(AF_INET, text.c_str(), &out) == 1;
}

struct Ipv4Net {
    uint32_t network = 0;
    uint32_t mask = 0;
    uint32_t broadcast = 0;
    uint32_t local = 0;
};

std::vector<Ipv4Net> local_ipv4_nets()
{
    std::vector<Ipv4Net> nets;
    ifaddrs *addrs = nullptr;
    if (getifaddrs(&addrs) != 0) return nets;
    for (ifaddrs *ifa = addrs; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (!ifa->ifa_netmask) continue;
        unsigned flags = ifa->ifa_flags;
        if (!(flags & IFF_UP) || (flags & IFF_LOOPBACK) || !(flags & IFF_BROADCAST)) continue;
        const auto *addr = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
        const auto *mask = reinterpret_cast<sockaddr_in *>(ifa->ifa_netmask);
        const auto *brd = reinterpret_cast<sockaddr_in *>(ifa->ifa_broadaddr);
        Ipv4Net net;
        net.local = ntohl(addr->sin_addr.s_addr);
        net.mask = ntohl(mask->sin_addr.s_addr);
        if (net.mask == 0 || net.mask < 0xFFFF0000UL) continue;
        net.network = net.local & net.mask;
        net.broadcast = brd ? ntohl(brd->sin_addr.s_addr) : (net.network | ~net.mask);
        bool exists = false;
        for (size_t i = 0; i < nets.size(); ++i) {
            if (nets[i].network == net.network && nets[i].mask == net.mask) {
                exists = true;
                break;
            }
        }
        if (!exists) nets.push_back(net);
    }
    freeifaddrs(addrs);
    return nets;
}

std::string ipv4_to_text(uint32_t host_order)
{
    in_addr addr;
    addr.s_addr = htonl(host_order);
    char buf[INET_ADDRSTRLEN] = {0};
    if (!inet_ntop(AF_INET, &addr, buf, sizeof(buf))) return "";
    return buf;
}

bool send_udp_packet(int sock, uint32_t host_order_ip, int port, const char *msg)
{
    sockaddr_in dest;
    std::memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(static_cast<uint16_t>(port));
    dest.sin_addr.s_addr = htonl(host_order_ip);
    const ssize_t n = sendto(sock, msg, std::strlen(msg), 0,
                             reinterpret_cast<sockaddr *>(&dest), sizeof(dest));
    return n >= 0;
}

void collect_udp_replies(int sock, int timeout_ms, LanServerType hint,
                        std::vector<LanServer> &servers)
{
    int remaining = timeout_ms;
    while (remaining > 0) {
        pollfd pfd;
        pfd.fd = sock;
        pfd.events = POLLIN;
        pfd.revents = 0;
        const int rc = poll(&pfd, 1, remaining);
        if (rc <= 0) break;
        sockaddr_in from;
        socklen_t from_len = sizeof(from);
        char buf[2048];
        const ssize_t n = recvfrom(sock, buf, sizeof(buf) - 1, 0,
                                   reinterpret_cast<sockaddr *>(&from), &from_len);
        if (n <= 0) break;
        buf[n] = 0;
        char ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
        LanServer server;
        if (lan_parse_discovery_payload(buf, ip, server, hint)) servers.push_back(server);
        remaining = std::max(120, timeout_ms / 3);
    }
}

struct HttpProbe {
    int fd = -1;
    std::string ip;
    int port = 0;
    std::string request;
    std::string response;
    size_t sent = 0;
    bool connected = false;
    bool failed = false;
};

void close_probe(HttpProbe &probe)
{
    if (probe.fd >= 0) {
        close(probe.fd);
        probe.fd = -1;
    }
}

bool start_probe(HttpProbe &probe)
{
    probe.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (probe.fd < 0) return false;
    const int flags = fcntl(probe.fd, F_GETFL, 0);
    if (flags < 0 || fcntl(probe.fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close_probe(probe);
        return false;
    }
#ifdef SO_NOSIGPIPE
    int yes = 1;
    setsockopt(probe.fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
#endif
    sockaddr_in dest;
    std::memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(static_cast<uint16_t>(probe.port));
    if (inet_pton(AF_INET, probe.ip.c_str(), &dest.sin_addr) != 1) {
        close_probe(probe);
        return false;
    }
    const int rc = connect(probe.fd, reinterpret_cast<sockaddr *>(&dest), sizeof(dest));
    if (rc == 0) {
        probe.connected = true;
        return true;
    }
    if (errno == EINPROGRESS || errno == EINTR) return true;
    close_probe(probe);
    return false;
}

bool finish_connect(HttpProbe &probe)
{
    int err = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(probe.fd, SOL_SOCKET, SO_ERROR, &err, &len) != 0 || err != 0) {
        probe.failed = true;
        close_probe(probe);
        return false;
    }
    probe.connected = true;
    return true;
}

bool pump_probe(HttpProbe &probe)
{
    if (probe.fd < 0 || probe.failed) return false;
    if (!probe.connected && !finish_connect(probe)) return false;
    if (probe.sent < probe.request.size()) {
        const char *data = probe.request.data() + probe.sent;
        const size_t left = probe.request.size() - probe.sent;
        const ssize_t n = send(probe.fd, data, left, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return true;
            probe.failed = true;
            close_probe(probe);
            return false;
        }
        probe.sent += static_cast<size_t>(n);
        return true;
    }
    char buf[1024];
    const ssize_t n = recv(probe.fd, buf, sizeof(buf), 0);
    if (n > 0) {
        probe.response.append(buf, static_cast<size_t>(n));
        return true;
    }
    if (n == 0) {
        close_probe(probe);
        return false;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return true;
    probe.failed = true;
    close_probe(probe);
    return false;
}

std::string http_body(const std::string &response)
{
    size_t pos = response.find("\r\n\r\n");
    size_t skip = 4;
    if (pos == std::string::npos) {
        pos = response.find("\n\n");
        skip = 2;
    }
    if (pos == std::string::npos) return "";
    return response.substr(pos + skip);
}

void harvest_probe(HttpProbe &probe, std::vector<LanServer> &servers)
{
    if (probe.failed || probe.response.empty()) return;
    const std::string body = http_body(probe.response);
    if (body.empty()) return;
    const std::string url = "http://" + probe.ip + ":" + std::to_string(probe.port);
    LanServer server;
    if (lan_parse_public_info(body, url, server)) servers.push_back(server);
}

void run_http_wave(std::vector<HttpProbe> &probes, std::vector<LanServer> &servers)
{
    std::vector<HttpProbe *> live;
    live.reserve(probes.size());
    for (size_t i = 0; i < probes.size(); ++i) {
        if (start_probe(probes[i])) live.push_back(&probes[i]);
    }

    const int budget = kHttpConnectMs + kHttpReadMs;
    int elapsed = 0;
    while (!live.empty() && elapsed < budget) {
        std::vector<pollfd> fds;
        fds.reserve(live.size());
        for (size_t i = 0; i < live.size(); ++i) {
            pollfd pfd;
            pfd.fd = live[i]->fd;
            pfd.events = POLLIN;
            if (!live[i]->connected || live[i]->sent < live[i]->request.size()) pfd.events |= POLLOUT;
            pfd.revents = 0;
            fds.push_back(pfd);
        }
        const int slice = std::min(80, budget - elapsed);
        const int rc = poll(fds.data(), fds.size(), slice);
        elapsed += slice;
        if (rc < 0 && errno == EINTR) continue;
        if (rc <= 0) continue;
        std::vector<HttpProbe *> next;
        next.reserve(live.size());
        for (size_t i = 0; i < live.size(); ++i) {
            if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                if (!live[i]->response.empty()) harvest_probe(*live[i], servers);
                close_probe(*live[i]);
                continue;
            }
            if ((fds[i].revents & (POLLIN | POLLOUT)) && pump_probe(*live[i])) {
                next.push_back(live[i]);
                continue;
            }
            harvest_probe(*live[i], servers);
            close_probe(*live[i]);
        }
        live.swap(next);
    }
    for (size_t i = 0; i < live.size(); ++i) {
        harvest_probe(*live[i], servers);
        close_probe(*live[i]);
    }
}

std::vector<LanServer> http_scan_subnet(const std::vector<Ipv4Net> &nets)
{
    std::vector<LanServer> servers;
    std::vector<HttpProbe> wave;
    wave.reserve(kHttpWaveSize);

    for (size_t n = 0; n < nets.size(); ++n) {
        const uint32_t first = nets[n].network + 1;
        const uint32_t last = nets[n].broadcast > 0 ? nets[n].broadcast - 1 : first;
        if (last < first || last - first > 1022) continue;
        for (uint32_t host = first; host <= last; ++host) {
            if (host == nets[n].local) continue;
            const std::string ip = ipv4_to_text(host);
            if (ip.empty()) continue;
            for (size_t p = 0; p < sizeof(kHttpPorts) / sizeof(kHttpPorts[0]); ++p) {
                HttpProbe probe;
                probe.ip = ip;
                probe.port = kHttpPorts[p];
                std::ostringstream req;
                req << "GET /System/Info/Public HTTP/1.1\r\n"
                    << "Host: " << ip << ":" << probe.port << "\r\n"
                    << "Accept: application/json\r\n"
                    << "Connection: close\r\n"
                    << "User-Agent: StreamPlayer-LANScan\r\n\r\n";
                probe.request = req.str();
                wave.push_back(probe);
                if (static_cast<int>(wave.size()) >= kHttpWaveSize) {
                    run_http_wave(wave, servers);
                    wave.clear();
                }
            }
        }
    }
    if (!wave.empty()) run_http_wave(wave, servers);
    return servers;
}

}  // namespace

std::string lan_normalize_base_url(std::string url)
{
    url = trim_copy(url);
    if (url.empty()) return "";
    if (url.find("://") == std::string::npos) url = "http://" + url;
    while (url.size() > 8 && url.back() == '/') url.pop_back();
    return url;
}

std::string lan_server_type_name(LanServerType type)
{
    switch (type) {
    case LanServerType::Jellyfin: return "Jellyfin";
    case LanServerType::Emby: return "Emby";
    case LanServerType::Unknown: break;
    }
    return "Server";
}

std::string lan_server_label(const LanServer &server)
{
    std::string name = server.name.empty() ? host_from_url(server.address) : server.name;
    if (name.empty()) name = server.address;
    return lan_server_type_name(server.type) + "  " + name;
}

bool lan_parse_discovery_payload(const std::string &payload,
                                 const std::string &from_ip,
                                 LanServer &out,
                                 LanServerType hint)
{
    if (!looks_like_json_object(payload)) return false;
    out = LanServer();
    out.address = lan_normalize_base_url(json_get_string(payload, "Address"));
    if (out.address.empty()) {
        out.address = lan_normalize_base_url(json_get_string(payload, "LocalAddress"));
    }
    if (out.address.empty() && !from_ip.empty()) {
        out.address = lan_normalize_base_url(from_ip + ":8096");
    }
    out.name = json_get_string(payload, "Name");
    if (out.name.empty()) out.name = json_get_string(payload, "ServerName");
    out.id = json_get_string(payload, "Id");
    out.type = type_from_text(payload + " " + out.name + " " + out.address, hint);
    return !out.address.empty();
}

bool lan_parse_public_info(const std::string &body,
                           const std::string &fallback_url,
                           LanServer &out)
{
    if (!looks_like_json_object(body)) return false;
    out = LanServer();
    out.address = lan_normalize_base_url(json_get_string(body, "LocalAddress"));
    if (out.address.empty()) out.address = lan_normalize_base_url(json_get_string(body, "Address"));
    if (out.address.empty()) out.address = lan_normalize_base_url(fallback_url);
    out.name = json_get_string(body, "ServerName");
    if (out.name.empty()) out.name = json_get_string(body, "Name");
    out.id = json_get_string(body, "Id");
    out.type = type_from_text(json_get_string(body, "ProductName"), type_from_text(body));
    return !out.address.empty() && (!out.name.empty() || !out.id.empty() ||
                                    body.find("ServerName") != std::string::npos ||
                                    body.find("ProductName") != std::string::npos);
}

void lan_dedupe_servers(std::vector<LanServer> &servers)
{
    std::vector<LanServer> unique;
    unique.reserve(servers.size());
    for (size_t i = 0; i < servers.size(); ++i) {
        LanServer item = servers[i];
        item.address = lan_normalize_base_url(item.address);
        if (item.address.empty()) continue;
        bool merged = false;
        for (size_t j = 0; j < unique.size(); ++j) {
            const bool same_id = !item.id.empty() && item.id == unique[j].id;
            const bool same_url = to_lower_copy(item.address) == to_lower_copy(unique[j].address);
            const bool same_host = host_from_url(item.address) == host_from_url(unique[j].address);
            if (!same_id && !same_url && !same_host) continue;
            if (unique[j].name.empty()) unique[j].name = item.name;
            if (unique[j].id.empty()) unique[j].id = item.id;
            if (unique[j].type == LanServerType::Unknown) unique[j].type = item.type;
            if (unique[j].address.find("://") == std::string::npos &&
                item.address.find("://") != std::string::npos) {
                unique[j].address = item.address;
            }
            merged = true;
            break;
        }
        if (!merged) unique.push_back(item);
    }
    std::sort(unique.begin(), unique.end(), [](const LanServer &a, const LanServer &b) {
        if (a.type != b.type) return static_cast<int>(a.type) < static_cast<int>(b.type);
        if (a.name != b.name) return a.name < b.name;
        return a.address < b.address;
    });
    servers.swap(unique);
}

std::vector<LanServer> lan_scan_servers(int udp_timeout_ms, bool http_fallback)
{
    std::vector<LanServer> servers;
    const std::vector<Ipv4Net> nets = local_ipv4_nets();

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        int yes = 1;
        setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        const int flags = fcntl(sock, F_GETFL, 0);
        if (flags >= 0) fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        sockaddr_in self;
        std::memset(&self, 0, sizeof(self));
        self.sin_family = AF_INET;
        self.sin_port = htons(0);
        self.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(sock, reinterpret_cast<sockaddr *>(&self), sizeof(self)) == 0) {
            struct DiscoveryQuery {
                const char *message;
                LanServerType type;
            };
            const DiscoveryQuery queries[] = {
                {"Who is JellyfinServer?", LanServerType::Jellyfin},
                {"who is EmbyServer?", LanServerType::Emby},
            };
            const int slice = std::max(350, udp_timeout_ms / 2);
            for (size_t q = 0; q < sizeof(queries) / sizeof(queries[0]); ++q) {
                send_udp_packet(sock, 0xFFFFFFFFu, kDiscoveryPort, queries[q].message);
                for (size_t n = 0; n < nets.size(); ++n) {
                    send_udp_packet(sock, nets[n].broadcast, kDiscoveryPort, queries[q].message);
                }
                collect_udp_replies(sock, slice, queries[q].type, servers);
            }
        }
        close(sock);
    }

    lan_dedupe_servers(servers);
    if (servers.empty() && http_fallback) {
        std::vector<LanServer> http_servers = http_scan_subnet(nets);
        servers.insert(servers.end(), http_servers.begin(), http_servers.end());
        lan_dedupe_servers(servers);
    }
    return servers;
}
