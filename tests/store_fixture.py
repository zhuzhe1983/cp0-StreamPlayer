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
args = parser.parse_args()


def item(audio=False):
    return {"Id": "a1" if audio else "v1", "Name": "QA audio" if audio else "QA test pattern",
            "Type": "Audio" if audio else "Movie", "MediaType": "Audio" if audio else "Video",
            "RunTimeTicks": 450000000, "Width": 320, "Height": 170,
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
        self.rfile.read(int(self.headers.get("Content-Length", "0")))
        path = urlsplit(self.path).path.removeprefix("/emby")
        if path.endswith("/AuthenticateByName"):
            return self.send_json({"AccessToken": "public-qa-token", "User": {"Id": "qa"}})
        if path.endswith("/PlaybackInfo"):
            return self.send_json({"PlaySessionId": "qa-session", "MediaSources": [{
                "Id": "qa-video", "SupportsTranscoding": True, "Width": 320, "Height": 170,
                "TranscodingUrl": "/media/stream.m3u8", "MediaStreams": [
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
            return self.send_json({"Items": [item(audio)], "TotalRecordCount": 1})
        if path.startswith("/media/") or path.startswith("/Audio/"):
            name = "audio.mp3" if path.startswith("/Audio/") else Path(path).name
            file = args.media / name
            if not file.is_file() or file.parent.resolve() != args.media.resolve():
                return self.send_error(404)
            data = file.read_bytes()
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
