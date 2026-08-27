# CurseForge external download tool protocol

Luna Launcher can delegate CurseForge files that are unavailable to third-party launchers to a user-configured executable. The launcher still validates every downloaded file before importing it.

## Capability probe

The settings page invokes the executable directly, without a shell:

```text
tool --probe
```

The tool must exit with code `0` within five seconds and write exactly one JSON object to stdout:

```json
{
  "protocolVersion": 1,
  "capabilities": ["curseforgeRestrictedDownload"],
  "headless": true
}
```

`headless` declares whether the tool can honor requests whose `mode` is `cli`. Diagnostics belong on stderr.

## Download request

For each restricted-download queue, the launcher creates a unique temporary directory and starts:

```text
tool --request <absolute-request.json>
```

The request has this form:

```json
{
  "protocolVersion": 1,
  "requestId": "65df99e4-37d6-45c8-a9e0-45a928bc83c7",
  "mode": "gui",
  "downloadDirectory": "D:\\Temp\\luna-curseforge-download-abcd",
  "items": [
    {
      "url": "https://www.curseforge.com/minecraft/mc-mods/example/download/123",
      "fileName": "example.jar",
      "hashAlgorithm": "sha1",
      "hash": "0123456789abcdef"
    }
  ],
  "startIndex": 1,
  "totalItems": 1,
  "proxy": {
    "type": "Default",
    "host": "",
    "port": 0
  }
}
```

The tool must write files directly under `downloadDirectory` using the requested `fileName`. Paths outside that directory are never accepted. Temporary files should use a different suffix until the final file is ready.

## Events and acknowledgement

Stdout is a newline-delimited JSON protocol. Each event for an item includes its requested `fileName`, one-based `fileIndex`, and `fileCount`:

```json
{"event":"downloadStarted","fileName":"example.jar","fileIndex":1,"fileCount":1}
{"event":"downloadProgress","fileName":"example.jar","fileIndex":1,"fileCount":1,"bytesReceived":1024,"bytesPerSecond":512}
{"event":"fileComplete","fileName":"example.jar","fileIndex":1,"fileCount":1}
```

After `fileComplete`, the tool must keep running and must not advance the queue. Luna checks the expected file and hash, preserves the verified file, and then writes this JSON line to the tool's stdin:

```json
{"command":"acceptCurrent","fileName":"example.jar"}
```

The tool may then emit `downloadFinished` and advance to the next item. It exits with code `0` only after every item has been acknowledged. Exit code `3` requests that the launcher restart the current item. Other nonzero codes are failures.

Failures may be reported before exit:

```json
{"event":"error","message":"description"}
```

The launcher considers the operation successful only when the process exits normally with code `0` and every requested file has been independently verified and acknowledged. A directory change, expected filename, event, or exit code is never sufficient by itself.

## Cancellation and lifetime

The tool is a per-request child process, not a daemon. When the user cancels or the owning task is destroyed, Luna first terminates it and then kills it if it does not stop promptly. The tool must not leave background processes behind.
