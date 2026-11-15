# Library download patches

This feature is off by default. Enable **Library download patches** in download
settings (Neo UI and the classic UI both provide the switch and file editors).
It applies to missing library JARs, including native classifiers, with Qt,
PCL.Download and aria2. Valid files bundled with an instance take priority.
Cached valid files are not forcibly downloaded again.

The base file is `library-download-patches.json5` beside the launcher executable.
The optional `library-download-patches.overlay.json5` lives in the active launcher
data directory (including portable UserData). Updates may replace the base file;
put personal changes in the overlay. Save edits before starting the next task.
The settings page shows both exact paths, merged rules and parsing errors.

```json5
{
  formatVersion: 1,
  rules: [
    {
      id: 'my-library',
      match: {
        library: 'example.group:artifact:1.0', // Exact name from version JSON.
        // classifier: 'natives-windows', // Optional resolved classifier.
        // url: 'https://old.example/artifact.jar', // Optional original URL.
      },
      mode: 'fallback', // 'replace' discards the original/mirror candidates.
      urls: ['https://new.example/artifact.jar'], // Ordered alternatives.
    },
  ],
}
```

All supplied match fields must match exactly. A library or original URL is
required; no wildcard, regular expression or script is executed. A library match
works even when version metadata has no explicit URL: it matches before the
generated Maven URL is downloaded. An URL match sees the original upstream URL
before mirror rewriting. Alternatives are exact HTTP(S) URLs and retain the same
storage destination and checksum validators. They must represent the same file.
Original valid source checksums are never removed or replaced by patches.

Rules run in file order. The overlay replaces an entire base rule with the same
`id`, preserving that rule's position; new IDs append. To disable a shipped rule:

```json5
{ formatVersion: 1, rules: [{ id: 'gtnh-lwjgl3-3.4.3-windows', enabled: false }] }
```

An invalid file layer is rejected with a visible error; valid rules from the
other file remain available. Files are limited to 1 MiB and the parser has time
and memory limits. JSON5 comments, unquoted keys, single quotes and trailing
commas are supported by the upstream JSON5 parser.

The GTNH example rule is disabled separately from the global switch. Its proposed
URL contains **core LWJGL** only, whereas the supplied GTNH pack's 16,483,083-byte
JAR contains 23 DLLs, including OpenAL, SDL3 and shaderc. These are not equivalent
files. Prefer the complete bundled JAR; replace the example URL with a verified
complete bundle before enabling that rule. Turning on the global switch alone
does not activate disabled rules.
