#include "hls_seek.h"

#include <cassert>
#include <iostream>
#include <string>

int main()
{
    const std::string master =
        "#EXTM3U\n"
        "#EXT-X-VERSION:3\n"
        "#EXT-X-STREAM-INF:BANDWIDTH=3928000\n"
        "main.m3u8?api_key=secret&StartTimeTicks=70000000\n";
    std::string variant;
    assert(hls_master_variant_uri(
        master, "http://media.local/videos/42/master.m3u8?api_key=secret", variant));
    assert(variant ==
           "http://media.local/videos/42/main.m3u8?api_key=secret&StartTimeTicks=70000000");

    const std::string media =
        "#EXTM3U\r\n"
        "#EXT-X-VERSION:3\r\n"
        "#EXT-X-TARGETDURATION:3\r\n"
        "#EXT-X-MEDIA-SEQUENCE:0\r\n"
        "#EXT-X-START:TIME-OFFSET=7\r\n"
        "#EXT-X-KEY:METHOD=AES-128,URI=\"key.bin\"\r\n"
        "#EXTINF:3.0000, nodesc\r\n"
        "0.ts?api_key=secret\r\n"
        "#EXT-X-MAP:URI=\"init.mp4\"\r\n"
        "#EXT-X-KEY:METHOD=AES-128,URI=\"rotated.key\"\r\n"
        "#EXTINF:3.0000, nodesc\r\n"
        "1.ts?api_key=secret\r\n"
        "#EXTINF:3.0000, nodesc\r\n"
        "2.ts?api_key=secret\r\n"
        "#EXTINF:3.0000, nodesc\r\n"
        "3.ts?api_key=secret\r\n"
        "#EXT-X-ENDLIST\r\n";
    HlsSeekPlaylist trimmed = hls_trim_media_playlist(
        media, "http://media.local/videos/42/main.m3u8?api_key=secret", 7);
    assert(trimmed.ok);
    assert(trimmed.skipped_segments == 2);
    assert(trimmed.actual_start_seconds == 6);
    assert(trimmed.playlist.find("#EXT-X-START") == std::string::npos);
    assert(trimmed.playlist.find("#EXT-X-MEDIA-SEQUENCE:2") != std::string::npos);
    assert(trimmed.playlist.find("http://media.local/videos/42/2.ts?api_key=secret") !=
           std::string::npos);
    assert(trimmed.playlist.find("http://media.local/videos/42/0.ts?api_key=secret") ==
           std::string::npos);
    assert(trimmed.playlist.find("URI=\"http://media.local/videos/42/key.bin\"") !=
           std::string::npos);
    assert(trimmed.playlist.find("URI=\"http://media.local/videos/42/init.mp4\"") !=
           std::string::npos);
    assert(trimmed.playlist.find("URI=\"http://media.local/videos/42/rotated.key\"") !=
           std::string::npos);
    assert(trimmed.playlist.find("#EXT-X-ENDLIST") != std::string::npos);

    HlsSeekPlaylist near_end = hls_trim_media_playlist(
        media, "http://media.local/videos/42/main.m3u8", 999);
    assert(near_end.ok);
    assert(near_end.skipped_segments == 3);
    assert(near_end.actual_start_seconds == 9);
    assert(near_end.playlist.find("/3.ts?api_key=secret") != std::string::npos);

    HlsSeekPlaylist invalid =
        hls_trim_media_playlist("not a playlist", "http://media.local/a.m3u8", 10);
    assert(!invalid.ok);

    std::cout << "hls_seek_test: ok\n";
    return 0;
}
