#include "hls_seek.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace {

struct Segment {
    std::size_t block_start = 0;
    std::size_t uri_line = 0;
    double duration = 0.0;
};

static std::string trim_copy(const std::string &value)
{
    std::size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

static bool starts_with(const std::string &value, const char *prefix)
{
    const std::size_t size = std::char_traits<char>::length(prefix);
    return value.size() >= size && value.compare(0, size, prefix) == 0;
}

static std::vector<std::string> playlist_lines(const std::string &playlist)
{
    std::vector<std::string> lines;
    std::istringstream input(playlist);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

static std::string strip_query_and_fragment(const std::string &url)
{
    const std::size_t delimiter = url.find_first_of("?#");
    return delimiter == std::string::npos ? url : url.substr(0, delimiter);
}

static std::string http_origin(std::string url)
{
    const auto scheme = url.find("://");
    if (scheme == std::string::npos) return {};
    const auto end = url.find_first_of("/?#", scheme + 3);
    if (end != std::string::npos) url.resize(end);
    for (char &c : url) c = std::tolower(static_cast<unsigned char>(c));
    return starts_with(url, "http://") || starts_with(url, "https://") ? url : "";
}

static std::string authorized_uri(const std::string &base_url,
                                   const std::string &reference,
                                   const std::string &access_token)
{
    std::string uri = hls_resolve_uri(base_url, reference);
    // Local HLS inputs cannot consume FFmpeg's HTTP-only -headers option.
    // Keep authentication in the private playlist, restricted to the server
    // origin. Keep existing API keys and do not send the token to another host.
    if (access_token.empty() || http_origin(base_url).empty() ||
        http_origin(uri) != http_origin(base_url)) return uri;
    const auto fragment = uri.find('#');
    const std::string suffix = fragment == std::string::npos ? "" : uri.substr(fragment);
    if (fragment != std::string::npos) uri.resize(fragment);
    const auto query = uri.find('?');
    if (query != std::string::npos) {
        std::istringstream parts(uri.substr(query + 1));
        std::string part;
        while (std::getline(parts, part, '&')) {
            auto key = part.substr(0, part.find('='));
            for (char &c : key) c = std::tolower(static_cast<unsigned char>(c));
            if (key == "api_key") return uri + suffix;
        }
    }
    std::string encoded;
    const char *hex = "0123456789ABCDEF";
    for (unsigned char c : access_token) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else {
            encoded += '%'; encoded += hex[c >> 4]; encoded += hex[c & 15];
        }
    }
    return uri + (query == std::string::npos ? "?" : "&") + "api_key=" + encoded + suffix;
}

static std::string replace_uri_attributes(const std::string &line,
                                          const std::string &base_url,
                                          const std::string &access_token)
{
    std::string result = line;
    std::size_t search_from = 0;
    while (true) {
        const std::size_t key = result.find("URI=\"", search_from);
        if (key == std::string::npos) break;
        const std::size_t value_begin = key + 5;
        const std::size_t value_end = result.find('"', value_begin);
        if (value_end == std::string::npos) break;
        const std::string resolved =
            authorized_uri(base_url,
                            result.substr(value_begin, value_end - value_begin), access_token);
        result.replace(value_begin, value_end - value_begin, resolved);
        search_from = value_begin + resolved.size() + 1;
    }
    return result;
}

static std::string normalized_output_line(const std::string &line,
                                          const std::string &base_url,
                                          const std::string &access_token)
{
    const std::string trimmed = trim_copy(line);
    if (trimmed.empty()) return std::string();
    if (trimmed[0] != '#') return authorized_uri(base_url, trimmed, access_token);
    return replace_uri_attributes(line, base_url, access_token);
}

static double extinf_duration(const std::string &line)
{
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos) return 0.0;
    const char *begin = line.c_str() + colon + 1;
    char *end = nullptr;
    const double duration = std::strtod(begin, &end);
    return end == begin || duration < 0.0 ? 0.0 : duration;
}

static long long media_sequence_value(const std::string &line)
{
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos) return 0;
    char *end = nullptr;
    const long long value = std::strtoll(line.c_str() + colon + 1, &end, 10);
    return end == line.c_str() + colon + 1 ? 0 : std::max<long long>(0, value);
}

}  // namespace

std::string hls_local_seek_playlist_path(const std::string &url)
{
    const std::string prefix = "file:///tmp/m5streamplayer-seek-";
    if (url.size() != prefix.size() + 6 + 5 ||
        url.compare(0, prefix.size(), prefix) != 0 ||
        url.compare(url.size() - 5, 5, ".m3u8") != 0) return {};
    for (std::size_t i = prefix.size(); i < prefix.size() + 6; ++i) {
        if (!std::isalnum(static_cast<unsigned char>(url[i]))) return {};
    }
    return url.substr(7);  // file://
}

std::vector<std::string> hls_seek_input_options(const std::string &url, bool mpv)
{
    if (hls_local_seek_playlist_path(url).empty()) return {};
    const std::string protocols = "file,http,https,tcp,tls,crypto,data";
    if (mpv) {
        // mpv's key/value parser needs a quoted comma-containing value.
        return {"--demuxer-lavf-o=protocol_whitelist=%" +
                std::to_string(protocols.size()) + "%" + protocols};
    }
    return {"-protocol_whitelist", protocols};
}

std::string hls_resolve_uri(const std::string &base_url, const std::string &reference)
{
    const std::string ref = trim_copy(reference);
    if (ref.empty()) return ref;
    if (starts_with(ref, "http://") || starts_with(ref, "https://") ||
        starts_with(ref, "file://")) {
        return ref;
    }

    const std::size_t scheme_end = base_url.find("://");
    if (scheme_end == std::string::npos) return ref;
    const std::size_t authority_end = base_url.find('/', scheme_end + 3);
    const std::string origin = authority_end == std::string::npos
        ? base_url
        : base_url.substr(0, authority_end);
    if (starts_with(ref, "//")) {
        return base_url.substr(0, scheme_end) + ":" + ref;
    }
    if (ref[0] == '/') return origin + ref;

    const std::string base_path = strip_query_and_fragment(base_url);
    const std::size_t slash = base_path.find_last_of('/');
    if (slash == std::string::npos) return origin + "/" + ref;
    return base_path.substr(0, slash + 1) + ref;
}

bool hls_master_variant_uri(const std::string &playlist,
                            const std::string &master_url,
                            std::string &variant_url)
{
    const std::vector<std::string> lines = playlist_lines(playlist);
    bool waiting_for_uri = false;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string line = trim_copy(lines[i]);
        if (starts_with(line, "#EXT-X-STREAM-INF:")) {
            waiting_for_uri = true;
            continue;
        }
        if (!waiting_for_uri || line.empty() || line[0] == '#') continue;
        variant_url = hls_resolve_uri(master_url, line);
        return !variant_url.empty();
    }
    return false;
}

HlsSeekPlaylist hls_trim_media_playlist(const std::string &playlist,
                                        const std::string &media_url,
                                        long long requested_start_seconds,
                                        const std::string &access_token)
{
    HlsSeekPlaylist result;
    const std::vector<std::string> lines = playlist_lines(playlist);
    if (lines.empty() || trim_copy(lines.front()) != "#EXTM3U") {
        result.error = "invalid HLS media playlist";
        return result;
    }

    std::vector<Segment> segments;
    std::size_t first_block_start = lines.size();
    std::size_t next_block_start = lines.size();
    long long original_sequence = 0;
    bool has_media_sequence = false;

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string line = trim_copy(lines[i]);
        if (starts_with(line, "#EXT-X-MEDIA-SEQUENCE:")) {
            original_sequence = media_sequence_value(line);
            has_media_sequence = true;
        }
        if (!starts_with(line, "#EXTINF:")) continue;

        Segment segment;
        segment.block_start =
            segments.empty() ? i : std::min(next_block_start, i);
        segment.duration = extinf_duration(line);
        std::size_t uri = i + 1;
        while (uri < lines.size()) {
            const std::string candidate = trim_copy(lines[uri]);
            if (!candidate.empty() && candidate[0] != '#') break;
            ++uri;
        }
        if (uri >= lines.size()) {
            result.error = "HLS segment is missing its URI";
            return result;
        }
        segment.uri_line = uri;
        if (segments.empty()) first_block_start = segment.block_start;
        segments.push_back(segment);
        next_block_start = uri + 1;
    }

    if (segments.empty()) {
        result.error = "HLS media playlist has no segments";
        return result;
    }

    const double requested =
        static_cast<double>(std::max<long long>(0, requested_start_seconds));
    double skipped_duration = 0.0;
    std::size_t skipped = 0;
    while (skipped + 1 < segments.size() &&
           skipped_duration + segments[skipped].duration <= requested + 0.000001) {
        skipped_duration += segments[skipped].duration;
        ++skipped;
    }

    std::vector<std::string> output;
    output.reserve(lines.size() - segments[skipped].block_start +
                   first_block_start + 2);
    bool wrote_media_sequence = false;
    for (std::size_t i = 0; i < first_block_start; ++i) {
        const std::string trimmed = trim_copy(lines[i]);
        if (starts_with(trimmed, "#EXT-X-START:")) continue;
        if (starts_with(trimmed, "#EXT-X-MEDIA-SEQUENCE:")) {
            output.push_back("#EXT-X-MEDIA-SEQUENCE:" +
                             std::to_string(original_sequence +
                                            static_cast<long long>(skipped)));
            wrote_media_sequence = true;
            continue;
        }
        output.push_back(normalized_output_line(lines[i], media_url, access_token));
    }
    if (!wrote_media_sequence && (has_media_sequence || skipped > 0)) {
        output.push_back("#EXT-X-MEDIA-SEQUENCE:" +
                         std::to_string(original_sequence +
                                        static_cast<long long>(skipped)));
    }

    /*
     * EXT-X-KEY and EXT-X-MAP remain active until another tag replaces them.
     * If the latest declaration lived in a skipped segment block, carry it
     * forward so the first retained segment can still be decrypted/decoded.
     */
    std::string active_key;
    std::string active_map;
    for (std::size_t i = first_block_start;
         i < segments[skipped].block_start;
         ++i) {
        const std::string trimmed = trim_copy(lines[i]);
        if (starts_with(trimmed, "#EXT-X-KEY:")) active_key = lines[i];
        if (starts_with(trimmed, "#EXT-X-MAP:")) active_map = lines[i];
    }
    if (!active_key.empty()) {
        output.push_back(normalized_output_line(active_key, media_url, access_token));
    }
    if (!active_map.empty()) {
        output.push_back(normalized_output_line(active_map, media_url, access_token));
    }

    for (std::size_t i = segments[skipped].block_start; i < lines.size(); ++i) {
        const std::string trimmed = trim_copy(lines[i]);
        if (starts_with(trimmed, "#EXT-X-START:")) continue;
        output.push_back(normalized_output_line(lines[i], media_url, access_token));
    }

    std::ostringstream rewritten;
    for (std::size_t i = 0; i < output.size(); ++i) {
        rewritten << output[i] << '\n';
    }
    result.ok = true;
    result.playlist = rewritten.str();
    result.actual_start_seconds =
        static_cast<long long>(skipped_duration + 0.000001);
    result.skipped_segments = skipped;
    return result;
}
