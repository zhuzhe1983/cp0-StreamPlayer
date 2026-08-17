#ifndef STREAMPLAYER_HLS_SEEK_H
#define STREAMPLAYER_HLS_SEEK_H

#include <cstddef>
#include <string>

struct HlsSeekPlaylist {
    bool ok = false;
    std::string playlist;
    long long actual_start_seconds = 0;
    std::size_t skipped_segments = 0;
    std::string error;
};

std::string hls_resolve_uri(const std::string &base_url, const std::string &reference);

bool hls_master_variant_uri(const std::string &playlist,
                            const std::string &master_url,
                            std::string &variant_url);

HlsSeekPlaylist hls_trim_media_playlist(const std::string &playlist,
                                        const std::string &media_url,
                                        long long requested_start_seconds);

#endif
