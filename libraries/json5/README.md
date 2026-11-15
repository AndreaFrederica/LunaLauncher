JSON5 2.2.3 (MIT), https://github.com/json5/json5.

`parser.inc` wraps the unmodified UMD distribution from
https://unpkg.com/json5@2.2.3/dist/index.min.js in a C++ raw string.
It runs in the existing QuickJS runtime solely to call `JSON5.parse`.
Configuration text is passed as a string, never evaluated as JavaScript.

See `LICENSE.md` for the upstream license.
