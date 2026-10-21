# MPL-2.0 component licensing

The full license text is in [`LICENSE-MPL-2.0`](LICENSE-MPL-2.0). The project-wide
GPL-3.0 license in [`LICENSE`](LICENSE) remains applicable except where source
files carry a different SPDX license identifier.

The following Luna-authored source is licensed under MPL-2.0:

- Files marked `SPDX-License-Identifier: MPL-2.0` in `launcher/api/`.
- `launcher/cli/McpServer.cpp`, `launcher/cli/McpServer.h`,
  `launcher/cli/OperationService.cpp`, `launcher/cli/OperationService.h`,
  `launcher/cli/ScopedUserInteraction.h`, and `launcher/cli/UserInteraction.h`.
- Luna-authored Neo UI sources marked MPL-2.0 in `neo-ui/`, excluding the
  upstream XMCL source and port described below.
- `examples/neo-ui/launcher-api.ts`.

Third-party source retains its existing license: the XMCL UI sources in
`neo-ui/upstream/xmcl-keystone-ui/` and the adapted port in `neo-ui/src/xmcl-port/`
remain MIT-licensed. `launcher/api/CustomUiRuntime.cpp` and
`launcher/api/CustomUiRuntime.h` remain GPL-3.0-only because they contain code
adapted from Prism Launcher. The remainder of the repository continues under
the project-wide GPL-3.0 license unless a file says otherwise.
