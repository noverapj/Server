# Novera Server

Server-side source code for the Novera game project. This repository contains all server applications and shared libraries built with Visual Studio 2022 (v143 toolset) and managed via Premake5.

## Recent Changes

- **Toolset upgrade**: VS 2010 (v100) → VS 2022 (v143)
- **Boost removed**: All Boost 1.47/1.50 dependencies eliminated; replaced with C++11/14/17 standard library (`std::thread`, `std::mutex`, `std::condition_variable`, etc.)
- **libcurl upgraded**: 7.40.0 → 8.21.0 (static, Schannel backend)
- **OpenSSL removed**: SHA/HMAC replaced with Windows BCrypt (CNG) API
- **PDB routing**: Symbols output to `build/zone_novera/server/<config>/pdb/`
- **Winsock init fix**: `BeginSocket()` called before config load (login/relay) to replace implicit `WSAStartup` previously provided by `boost::asio`

## Projects

| Project | Type | Description |
|---------|------|-------------|
| ioINILoader | Static Lib | INI file parser |
| Log | Static Lib | Logging library |
| iocpSocketDLL | DLL | IOCP socket library |
| FrameTimerDLL | DLL | Frame timer library |
| LogDLL | DLL | Log sync library |
| LS_GoogleDump | Static Lib | Google breakpad crash dump |
| LS_NXSoap | Static Lib | SOAP client library (OpenSSL removed) |
| LS_HTTP | Static Lib | HTTP client library |
| tinyxml | Static Lib | TinyXML parser |
| LS_RestAPI | Static Lib | REST API library (libcurl 8.21, Schannel) |
| ls_loginsvr | App | Login server |
| ls_relaysvr | App | Relay server |
| ls_mainsvr | App | Main server |
| ls_gamesvr | App | Game server |
| ls_billingsvr | App | Billing server |
| ls_dbagent | App | Database agent server |
| ls_filewritesvr | App | File write server |

## Prerequisites

- Visual Studio 2022 with v143 platform toolset
- Windows SDK 10.0 (bundled with VS 2022)
- Premake5 (auto-downloaded by `build.bat`)
- CMake 3.20+ (only needed to rebuild libcurl; pre-built libs included)

## Building

### Quick start

```batch
build.bat                    # Generate VS2022 project files (auto-downloads premake5)
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
│   ├── libcurl/include/  # libcurl 8.21 headers
│   └── LS_RestAPI/       # REST API (links libcurl)
├── lib/                  # Build outputs (.lib, .dll)
│   └── curl/             # libcurl 8.21 static libs (libcurl.lib, libcurld.lib)
└── build/                # Generated VS2022 project files (gitignored)
```

## Build Configuration

- **CRT linkage**: Server apps static `/MT` (Release) / `/MTd` (Debug); shared DLLs dynamic `/MD` / `/MDd`
- **libcurl**: Statically linked, Schannel backend (no OpenSSL), `/MT` Release + `/MTd` Debug
- **PDB output**: Routed to `build/zone_novera/server/<config>/pdb/` via Premake `symbolspath`
- **Platform toolset**: v143 (VS 2022), target Windows 10+

## License

This project is licensed under the GNU General Public License v3.0 — see [LICENSE](LICENSE) for details.

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting pull requests.
