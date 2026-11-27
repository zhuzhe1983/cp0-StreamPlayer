#!/usr/bin/env python3
"""Local Emby/Jellyfin protocol fixture for physical-device playback checks.

Uses generated test media and public, disposable QA values, not real accounts.
It is a test service, not evidence of compatibility with a production server.
"""
import argparse
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--media", type=Path, required=True)
parser.add_argument("--bind", default="127.0.0.1")
parser.add_argument("--port", type=int, default=18751)
parser.add_argument("--audit-log", type=Path)
parser.add_argument("--duration", type=int, default=45)
parser.add_argument("--season-first", action="store_true",
                    help="Expose a Season item in recent items to test container navigation")
parser.add_argument("--require-media-auth", action="store_true",
                    help="Require the disposable token on playlists and segments")
args = parser.parse_args()


def item(audio=False):
    return {"Id": "a1" if audio else "v1", "Name": "QA audio" if audio else "QA test pattern",
            "Type": "Audio" if audio else "Movie", "MediaType": "Audio" if audio else "Video",
            "RunTimeTicks": args.duration * 10000000, "Width": 320, "Height": 170,
            "Container": "mp3" if audio else "mp4", "VideoCodec": "h264", "AudioCodec": "aac",
            "Overview": "Generated local test media for store-edition validation."}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *values):
        # No headers, passwords, or query tokens in the test log.
        print(self.command, urlsplit(self.path).path, flush=True)

    def send_json(self, value):
        data = json.dumps(value).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
        path = urlsplit(self.path).path.removeprefix("/emby")
        if path.endswith("/AuthenticateByName"):
            return self.send_json({"AccessToken": "public-qa-token", "User": {"Id": "qa"}})
        if path.endswith("/PlaybackInfo"):
            if args.season_first and path == "/Items/season1/PlaybackInfo":
                if args.audit_log:
                    with args.audit_log.open("a") as audit:
                        audit.write(json.dumps({"event": "invalid-season-playback"}) + "\n")
                return self.send_error(500, "A season is a directory, not a video")
            query = parse_qs(urlsplit(self.path).query)
            width = int(query.get("MaxWidth", ["320"])[0])
            height = int(query.get("MaxHeight", ["170"])[0])
            if args.audit_log:
                profile = json.loads(body or b"{}").get("DeviceProfile", {})
                # Deliberately omit credentials, tokens and arbitrary URLs.
                with args.audit_log.open("a") as audit:
                    audit.write(json.dumps({"event": "PlaybackInfo", "width": width,
                        "height": height, "fps": query.get("MaxFramerate"),
                        "video_bitrate": query.get("VideoBitrate"),
                        "audio_bitrate": query.get("AudioBitrate"),
                        "max_streaming_bitrate": query.get("MaxStreamingBitrate"),
                        "profile": profile.get("Name"),
                        "transcoding": profile.get("TranscodingProfiles")}) + "\n")
            playlist = "hdmi.m3u8" if width > 320 and (args.media / "hdmi.m3u8").is_file() else "stream.m3u8"
            return self.send_json({"PlaySessionId": "qa-session", "MediaSources": [{
                "Id": "qa-video", "SupportsTranscoding": True, "Width": width, "Height": height,
                "TranscodingUrl": "/media/" + playlist, "MediaStreams": [
                    {"Index": 0, "Type": "Video", "Codec": "h264"},
                    {"Index": 1, "Type": "Audio", "Codec": "aac", "Language": "eng"}]}]})
        return self.send_json({})

    def do_GET(self):
        url = urlsplit(self.path)
        path = url.path.removeprefix("/emby")
        query = parse_qs(url.query)
        if "/Images/" in path:
            return self.send_error(404)
        if path.endswith("/Views"):
            return self.send_json({"Items": [
                {"Id": "video", "Name": "QA Videos", "CollectionType": "movies"},
                {"Id": "music", "Name": "QA Music", "CollectionType": "music"}]})
        if path.endswith("/Items"):
            audio = query.get("ParentId") == ["music"]
            if args.season_first and not audio:
                return self.send_json({"Items": [{"Id": "season1", "Name": "QA Season 4",
                    "Type": "Season", "IsFolder": True, "ParentId": "series1"}],
                    "TotalRecordCount": 1})
            return self.send_json({"Items": [item(audio)], "TotalRecordCount": 1})
        if path == "/Shows/series1/Episodes":
            if args.audit_log:
                with args.audit_log.open("a") as audit:
                    audit.write(json.dumps({"event": "season-episodes",
                        "season_id": query.get("SeasonId")}) + "\n")
            episode = item()
            episode.update(Type="Episode", SeriesId="series1", SeasonId="season1")
            return self.send_json({"Items": [episode], "TotalRecordCount": 1})
        if path.startswith("/media/") or path.startswith("/Audio/"):
            auth = ("header" if self.headers.get("X-Emby-Token") == "public-qa-token"
                    else "query" if query.get("api_key") == ["public-qa-token"] else "missing")
            if args.require_media_auth:
                if args.audit_log:
                    with args.audit_log.open("a") as audit:
                        audit.write(json.dumps({"event": "media-auth", "path": path,
                                                "auth": auth}) + "\n")
                if auth == "missing":
                    return self.send_error(401)
            name = "audio.mp3" if path.startswith("/Audio/") else Path(path).name
            file = args.media / name
            if not file.is_file() or file.parent.resolve() != args.media.resolve():
                return self.send_error(404)
            data = file.read_bytes()
            ticks = int(query.get("StartTimeTicks", ["0"])[0])
            if name.endswith(".m3u8") and ticks > 0:
                # Model the server's seeked HLS response, retaining the
                # original segment timestamps for real position checks.
                lines = data.decode().splitlines()
                headers, segments = [], []
                pending = None
                for line in lines:
                    if line.startswith("#EXTINF:"):
                        pending = line
                    elif pending is not None and line and not line.startswith("#"):
                        segments.append((pending, line))
                        pending = None
                    elif not segments and pending is None and line != "#EXT-X-ENDLIST":
                        headers.append(line)
                elapsed, first = 0.0, 0
                while first < len(segments):
                    duration = float(segments[first][0].split(":", 1)[1].split(",", 1)[0])
                    if elapsed + duration > ticks / 10000000:
                        break
                    elapsed += duration
                    first += 1
                if first == len(segments):
                    return self.send_error(416)
                headers = ["#EXT-X-MEDIA-SEQUENCE:" + str(first)
                           if line.startswith("#EXT-X-MEDIA-SEQUENCE:") else line
                           for line in headers]
                data = ("\n".join(headers + [line for pair in segments[first:] for line in pair]
                                  + ["#EXT-X-ENDLIST"]) + "\n").encode()
                if args.audit_log:
                    with args.audit_log.open("a") as audit:
                        audit.write(json.dumps({"event": "playlist-seek", "playlist": name,
                            "requested_ticks": ticks, "first_segment_seconds": elapsed}) + "\n")
            start, end = 0, len(data) - 1
            partial = self.headers.get("Range", "")
            if partial.startswith("bytes="):
                spec = partial[6:].split("-", 1)
                start = int(spec[0] or "0")
                end = min(int(spec[1]) if spec[1] else end, end)
            self.send_response(206 if partial else 200)
            self.send_header("Content-Type", "application/vnd.apple.mpegurl" if name.endswith("m3u8") else "audio/mpeg" if name.endswith("mp3") else "video/mp2t")
            self.send_header("Accept-Ranges", "bytes")
            if partial:
                self.send_header("Content-Range", f"bytes {start}-{end}/{len(data)}")
            self.send_header("Content-Length", str(end - start + 1))
            self.end_headers()
            try:
                self.wfile.write(data[start:end + 1])
            except (BrokenPipeError, ConnectionResetError):
                pass
            return
        return self.send_json({})


ThreadingHTTPServer((args.bind, args.port), Handler).serve_forever()
