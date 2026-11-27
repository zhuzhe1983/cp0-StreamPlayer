#ifndef STREAMPLAYER_HLS_SEEK_H
#define STREAMPLAYER_HLS_SEEK_H

#include <cstddef>
#include <string>
#include <vector>

struct HlsSeekPlaylist {
    bool ok = false;
    std::string playlist;
    long long actual_start_seconds = 0;
    std::size_t skipped_segments = 0;
    std::string error;
};

std::string hls_resolve_uri(const std::string &base_url, const std::string &reference);

// Only playlists created by this application's mkstemps seek fallback receive
// permission to open network segments from a local file input.
std::string hls_local_seek_playlist_path(const std::string &url);
std::vector<std::string> hls_seek_input_options(const std::string &url, bool mpv);

bool hls_master_variant_uri(const std::string &playlist,
                            const std::string &master_url,
                            std::string &variant_url);

HlsSeekPlaylist hls_trim_media_playlist(const std::string &playlist,
                                        const std::string &media_url,
                                        long long requested_start_seconds,
                                        const std::string &access_token = {});

#endif
