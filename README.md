# Novera Server

Server-side source code for the Novera game project. This repository contains all server applications and shared libraries built with Visual Studio 2010 (v100 toolset) and managed via Premake5.

## Projects

| Project | Type | Description |
|---------|------|-------------|
| LSLog | DLL / Static | Logging library (DLL + static variants) |
| ioPac | DLL / Static | Pack file system library (DLL + static variants) |
| TownPortal | DLL / Static | Network portal library (DLL + static variants) |
| ioFreeType | DLL | FreeType font rendering wrapper |
| OggVorbis | Static Lib | OggVorbis audio codec built from source |
| ErrorDlg | Static Lib | Error dialog utility |
| LSDBAgent | App | Database agent server |
| LSFileWriteSvr | App | File write server |
| LSGateSvr | App | Gate server |
| LSGameSvr | App | Game server |
| LSLogClient | App | Log client |
| LSLoginSvr | App | Login server |
| LSMainSvr | App | Main server |
| LSNickNameSvr | App | Nickname server |
| LSPatchSvr | App | Patch server |
| LSBillingSvr | App | Billing server |
| LSRelaySvr | App | Relay server |
| LSStateSvr | App | State server |
| LSWebSvr | App | Web server |
| ioIocpSocketDLL | DLL | IOCP socket library |
| ioLangDLL | DLL | Language/localization library |

## Prerequisites

- Visual Studio 2010 (or VS 2022 with v100 platform toolset)
- Premake5 (beta 8)
- Windows SDK 7.0A

## Building

```batch
premake5.exe vs2010 --file=premake5.lua
msbuild build\SourceServer.sln /p:Configuration=Debug /p:Platform=Win32 /p:PlatformToolset=v100 /m
```

## Directory Structure

```
SourceServer/
├── premake5.lua          # Premake5 build configuration
├── scripts/              # Build helper scripts
├── src/                  # Source code
│   ├── ioIocpSocketDLL/  # IOCP socket library
│   ├── ioLangDLL/        # Language library
│   ├── ioPac/            # Pack file system
│   ├── ioFreeType/       # Font rendering
│   ├── LSLog/            # Logging
│   ├── TownPortal/       # Network portal
│   ├── OggVorbis/        # Audio codec
│   ├── ErrorDlg/         # Error dialogs
│   ├── LSDBAgent/        # Database agent server
│   ├── LSFileWriteSvr/   # File write server
│   ├── LSGateSvr/        # Gate server
│   ├── LSGameSvr/        # Game server
│   ├── LSLogClient/      # Log client
│   ├── LSLoginSvr/       # Login server
│   ├── LSMainSvr/        # Main server
│   ├── LSNickNameSvr/    # Nickname server
│   ├── LSPatchSvr/       # Patch server
│   ├── LSBillingSvr/     # Billing server
│   ├── LSRelaySvr/       # Relay server
│   ├── LSStateSvr/       # State server
│   └── LSWebSvr/         # Web server
├── ThirdParty/           # Third-party headers and libraries
├── lib/                  # Build outputs (.lib, .dll)
└── build/                # Generated VS2010 project files (gitignored)
```

## License

This project is licensed under the GNU General Public License v3.0 — see [LICENSE](LICENSE) for details.

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting pull requests.
