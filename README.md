# Novera Server

Server-side source code for the Novera game project. This repository contains all server applications and shared libraries built with Visual Studio 2010 (v100 toolset) and managed via Premake5.

## Projects

| Project | Type | Description |
|---------|------|-------------|
| ioINILoader | Static Lib | INI file parser |
| Log | Static Lib | Logging library |
| iocpSocketDLL | DLL | IOCP socket library |
| FrameTimerDLL | DLL | Frame timer library |
| LogDLL | DLL | Log sync library |
| LS_GoogleDump | Static Lib | Google breakpad crash dump |
| LS_NXSoap | Static Lib | SOAP client library |
| LS_HTTP | Static Lib | HTTP client library |
| tinyxml | Static Lib | TinyXML parser |
| LS_RestAPI | Static Lib | REST API library |
| ls_loginsvr | App | Login server |
| ls_relaysvr | App | Relay server |
| ls_mainsvr | App | Main server |
| ls_gamesvr | App | Game server |
| ls_billingsvr | App | Billing server |
| ls_dbagent | App | Database agent server |
| ls_filewritesvr | App | File write server |

## Prerequisites

- Visual Studio 2010 (or VS 2022 with v100 platform toolset)
- Windows SDK 7.0A
- Premake5 is auto-downloaded by `build.bat` (no manual install needed)

## Building

### Quick start

```batch
build.bat                    # Generate VS2010 project files (auto-downloads premake5)
scripts\build.bat Debug       # Build solution (Debug)
scripts\build.bat All         # Build Debug + Release
```

### Build single project

```batch
scripts\build_project.bat ls_gamesvr             # Debug (default)
scripts\build_project.bat ls_billingsvr Release   # Release config
scripts\build_project.bat                          # List available projects
```

## Directory Structure

```
SourceServer/
├── build.bat             # Premake5 generator (auto-downloads premake5)
├── premake5.lua          # Premake5 build configuration
├── scripts/              # Build scripts (build.bat, build_project.bat, gen_version.bat)
├── src/                  # Source code
│   ├── ioINILoader/      # INI file parser
│   ├── Log/              # Logging library
│   ├── iocpSocketDLL/    # IOCP socket library
│   ├── FrameTimerDLL/    # Frame timer library
│   ├── Log_Sync/         # Log sync DLL
│   ├── include/          # Shared headers
│   ├── ls_loginsvr/      # Login server
│   ├── ls_relaysvr/      # Relay server
│   ├── ls_mainsvr/       # Main server
│   ├── ls_gamesvr/       # Game server
│   ├── ls_billingsvr/    # Billing server
│   ├── ls_dbagent/       # Database agent server
│   └── ls_filewritesvr/  # File write server
├── ThirdParty/           # Third-party headers and libraries
├── lib/                  # Build outputs (.lib, .dll)
└── build/                # Generated VS2010 project files (gitignored)
```

## License

This project is licensed under the GNU General Public License v3.0 — see [LICENSE](LICENSE) for details.

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting pull requests.
