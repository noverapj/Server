-- NOVERAOSS-Server premake5
-- Toolset: VS2022 (v143). CRT: mixed (server apps static /MT, DLLs dynamic /MD).

local function Vpaths(projDir)
    vpaths {
        ["Source Files"]    = { projDir .. "/*.cpp" },
        ["Header Files"]    = { projDir .. "/*.h" },
        ["Resource Files"]  = { projDir .. "/*.rc" },
        ["*"]               = { projDir .. "/**.cpp", projDir .. "/**.h", projDir .. "/**.rc", projDir .. "/**.txt" },
    }
end

workspace "Server"
    configurations { "Debug", "Release" }
    platforms { "Win32" }
    toolset "v143"
    location "build"
    characterset "MBCS"
    multiprocessorcompile "On"
    buildoptions { "/std:c++17" }
    defines { "_HAS_STD_BYTE=0" }

    filter "configurations:Debug"
        defines { "_DEBUG" }
    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "Speed"
    filter {}

------------------------------------------------------------------ libs
group "Libs"
-- ioINILoader : static lib, /MT
project "ioINILoader"
    kind "StaticLib"
    language "C++"
    location "build"
    targetdir "lib"
    objdir "build/obj/%{cfg.buildcfg}/%{prj.name}"
    staticruntime "On"
    defines { "_LIB" }
    files { "src/ioINILoader/**.h", "src/ioINILoader/**.cpp" }
    Vpaths("src/ioINILoader")
    filter "configurations:Debug" runtime "Debug"; targetname "INID"
    filter "configurations:Release" runtime "Release"; targetname "INI"
    filter {}

-- Log : static lib, /MT, USE_THREAD
project "Log"
    kind "StaticLib"
    language "C++"
    location "build"
    targetdir "lib"
    objdir "build/obj/%{cfg.buildcfg}/%{prj.name}"
    staticruntime "On"
    defines { "USE_THREAD", "_LIB" }
    files { "src/Log/**.h", "src/Log/**.cpp" }
    removefiles { "src/Log/ioLogFile.cpp" }
    Vpaths("src/Log")
    filter "configurations:Debug" runtime "Debug"; targetname "LogD"
    filter "configurations:Release" runtime "Release"; targetname "Log"
    filter {}

-- iocpSocketDLL : DLL, /MD (Debug), /MT anomaly preserved on Release
project "iocpSocketDLL"
    kind "SharedLib"
    language "C++"
    location "build"
    targetdir "lib"
    objdir "build/obj/%{cfg.buildcfg}/%{prj.name}"
    defines { "EXPORT_IOCP_SOCKET", "_USRDLL", "IOCPSOCKETDLL_EXPORTS" }
    files { "src/iocpSocketDLL/**.h", "src/iocpSocketDLL/**.cpp", "src/iocpSocketDLL/**.rc" }
    Vpaths("src/iocpSocketDLL")
    filter "configurations:Debug"
        runtime "Debug"; targetname "iocpSocketDDLL"; staticruntime "Off"
        links { "ws2_32", "Psapi" }
    filter "configurations:Release"
        runtime "Release"; targetname "iocpSocketDLL"; staticruntime "On"
        links { "ws2_32" }
    filter {}
    prebuildcommands { '"$(ProjectDir)..\\scripts\\gen_version.bat" "$(ProjectDir)..\\src\\iocpSocketDLL" Version.h' }

-- FrameTimerDLL : DLL, /MD
project "FrameTimerDLL"
    kind "SharedLib"
    language "C++"
    location "build"
    targetdir "lib"
    objdir "build/obj/%{cfg.buildcfg}/%{prj.name}"
    defines { "FRAMETIMERDLL_EXPORTS", "_USRDLL" }
    files { "src/FrameTimerDLL/**.h", "src/FrameTimerDLL/**.cpp", "src/FrameTimerDLL/**.rc" }
    Vpaths("src/FrameTimerDLL")
    filter "configurations:Debug" runtime "Debug"; targetname "FrameTimerDDLL"; staticruntime "Off"
    filter "configurations:Release" runtime "Release"; targetname "FrameTimerDLL"; staticruntime "Off"
    filter {}

-- LogDLL (Log_Sync) : DLL, /MD
project "LogDLL"
    kind "SharedLib"
    language "C++"
    location "build"
    targetdir "lib"
    objdir "build/obj/%{cfg.buildcfg}/%{prj.name}"
    defines { "EXPORT_LOG_SYNC", "_USRDLL", "LOGDLL_EXPORTS" }
    files { "src/Log_Sync/**.h", "src/Log_Sync/**.cpp", "src/Log_Sync/**.rc" }
    Vpaths("src/Log_Sync")
    filter "configurations:Debug" runtime "Debug"; targetname "logddll"; staticruntime "Off"
    filter "configurations:Release" runtime "Release"; targetname "logdll"; staticruntime "Off"
    filter {}

------------------------------------------------------------------ ThirdParty
group "ThirdParty"

project "LS_GoogleDump"
    kind "StaticLib"
    language "C++"
    location "build"
    targetdir "lib"
    objdir "build/obj/%{cfg.buildcfg}/%{prj.name}"
    staticruntime "On"
    characterset "MBCS"
    files { "ThirdParty/LS_GoogleDump/LS_GoogleDump/**.h", "ThirdParty/LS_GoogleDump/LS_GoogleDump/**.cpp" }
    removefiles { "ThirdParty/LS_GoogleDump/LS_GoogleDump/ClientInfo.cpp", "ThirdParty/LS_GoogleDump/LS_GoogleDump/CrashServer.cpp" }
    filter "configurations:Debug" runtime "Debug"; targetname "LS_GoogleDumpD"
    filter "configurations:Release" runtime "Release"; targetname "LS_GoogleDump"
    filter {}

project "LS_NXSoap"
    kind "StaticLib"
    language "C++"
    location "build"
    targetdir "lib"
    objdir "build/obj/%{cfg.buildcfg}/%{prj.name}"
    staticruntime "On"
    characterset "MBCS"
    defines { "WITH_OPENSSL" }
    files { "ThirdParty/LS_NXSoap/LS_NXSoap/**.h", "ThirdParty/LS_NXSoap/LS_NXSoap/**.cpp" }
    includedirs { "ThirdParty/LS_NXSoap/openssl" }
    filter "configurations:Debug" runtime "Debug"; targetname "LS_NXSoapD"
    filter "configurations:Release" runtime "Release"; targetname "LS_NXSoap"
    filter {}

project "LS_HTTP"
    kind "StaticLib"
    language "C++"
    location "build"
    targetdir "lib"
    objdir "build/obj/%{cfg.buildcfg}/%{prj.name}"
    staticruntime "On"
    characterset "MBCS"
    files { "ThirdParty/LS_HTTP/LS_HTTP/**.h", "ThirdParty/LS_HTTP/LS_HTTP/**.cpp" }
    removefiles { "ThirdParty/LS_HTTP/LS_HTTP/sample.cpp" }
    includedirs { "ThirdParty/LS_HTTP/include" }
    filter "configurations:Debug" runtime "Debug"; targetname "LS_HTTPD"
    filter "configurations:Release" runtime "Release"; targetname "LS_HTTP"
    filter {}

project "tinyxml"
    kind "StaticLib"
    language "C++"
    location "build"
    targetdir "lib"
    objdir "build/obj/%{cfg.buildcfg}/%{prj.name}"
    staticruntime "On"
    characterset "MBCS"
    files { "ThirdParty/tinyxml/tinyxml/**.h", "ThirdParty/tinyxml/tinyxml/**.cpp" }
    removefiles { "ThirdParty/tinyxml/tinyxml/xmltest.cpp" }
    filter "configurations:Debug" runtime "Debug"; targetname "tinyxmlD"
    filter "configurations:Release" runtime "Release"; targetname "tinyxml"
    filter {}

project "LS_RestAPI"
    kind "StaticLib"
    language "C++"
    location "build"
    targetdir "lib"
    objdir "build/obj/%{cfg.buildcfg}/%{prj.name}"
    staticruntime "On"
    characterset "MBCS"
    pchheader "stdafx.h"; pchsource "ThirdParty/LS_RestAPI/stdafx.cpp"
    files { "ThirdParty/LS_RestAPI/**.h", "ThirdParty/LS_RestAPI/**.cpp" }
    includedirs { "ThirdParty/LS_RestAPI" }
    filter "configurations:Debug" runtime "Debug"; targetname "LS_RestAPID"
    filter "configurations:Release" runtime "Release"; targetname "LS_RestAPI"
    filter {}

------------------------------------------------------------------ ls_* helper
local function ls_app(name, dir, opts)
    project(name)
    kind "ConsoleApp"
    language "C++"
    location "build"
    targetdir "../build/zone_novera/server/%{cfg.buildcfg}/%{prj.name}"
    objdir "build/obj/%{cfg.buildcfg}/%{prj.name}"
    symbolspath '$(OutDir)../pdb/$(TargetName).pdb'
    staticruntime "On"
    characterset "MBCS"
    multiprocessorcompile "On"
    files { dir .. "/**.h", dir .. "/**.cpp", dir .. "/**.rc" }
    Vpaths(dir)
    includedirs { dir, "src/include", "ThirdParty" }
    libdirs { "lib" }
    filter "configurations:Debug"
        runtime "Debug"; defines "_DEBUG"; targetsuffix "D"
        links { "ioINILoader", "Log", "iocpSocketDLL", "FrameTimerDLL", "dbghelp", "ws2_32", "winmm", "Psapi", "odbc32", "odbccp32", "CrashFind" }
    filter "configurations:Release"
        runtime "Release"; defines "NDEBUG"; optimize "Speed"
        links { "ioINILoader", "Log", "iocpSocketDLL", "FrameTimerDLL", "dbghelp", "ws2_32", "winmm", "Psapi", "odbc32", "odbccp32", "CrashFind" }
    filter {}
    if opts then opts() end
    prebuildcommands { '"$(ProjectDir)..\\scripts\\gen_version.bat" "$(ProjectDir)..\\' .. dir .. '" Version.h' }
end

group "Servers"
-- ls_loginsvr (PCH)
ls_app("ls_loginsvr", "src/ls_loginsvr", function()
    pchheader "stdafx.h"; pchsource "src/ls_loginsvr/stdafx.cpp"
end)

-- ls_relaysvr (PCH)
ls_app("ls_relaysvr", "src/ls_relaysvr", function()
    pchheader "stdafx.h"; pchsource "src/ls_relaysvr/stdafx.cpp"
    removefiles { "src/ls_relaysvr/LSGameServerConnector/ServerInfoManager.cpp" }
end)

-- ls_mainsvr (boost 1.50, no PCH)
ls_app("ls_mainsvr", "src/ls_mainsvr", function()
    files { "src/ls_mainsvr/**.c" }
    removefiles { "src/ls_mainsvr/INILoader/ioDataChunk.cpp", "src/ls_mainsvr/INILoader/ioINILoader.cpp", "src/ls_mainsvr/INILoader/ioINIParser.cpp", "src/ls_mainsvr/INILoader/ioStream.cpp", "src/ls_mainsvr/Util/Utility.cpp" }
    defines { "__OHTG_PRACTICE_MERGE_WORK__" }
end)

-- ls_gamesvr (boost 1.50, PCH)
ls_app("ls_gamesvr", "src/ls_gamesvr", function()
    pchheader "stdafx.h"; pchsource "src/ls_gamesvr/stdafx.cpp"
    removefiles {
        "src/ls_gamesvr/INILoader/ioDataChunk.cpp", "src/ls_gamesvr/INILoader/ioINILoader.cpp", "src/ls_gamesvr/INILoader/ioINIParser.cpp", "src/ls_gamesvr/INILoader/ioStream.cpp",
        "src/ls_gamesvr/Local/ioLocalEU.cpp", "src/ls_gamesvr/Local/ioLocalLatin.cpp", "src/ls_gamesvr/Local/ioLocalSingapore.cpp",
        "src/ls_gamesvr/NodeInfo/BattleRoomReserveMgr.cpp", "src/ls_gamesvr/NodeInfo/GuildInven.cpp",
    }
    includedirs { "ThirdParty/NMCrypt", "ThirdParty/nProtect", "ThirdParty/Xtrap", "ThirdParty/XignCode", "ThirdParty/HackShield", "ThirdParty/CrashFind" }
    libdirs { "lib/nProtect" }
    links { "NMCrypt", "wininet" }
    ignoredefaultlibraries { "LIBC.lib" }
    defines { "__OHTG_BILLING_VALOFE_ADD__", "__OHTG_PRACTICE_MERGE_WORK__" }
    filter { "files:src/ls_gamesvr/Xtrap/XTrap4Server.cpp" }
        buildoptions { "/Y-" }
    filter "configurations:Release"
        defines { "__OHTG_PCBANG_EVENT_CASH__" }
        linkoptions { "/SAFESEH:NO" }
    filter {}
end)

-- ls_billingsvr (OpenSSL, no boost, no PCH, #import needs /MP off)
ls_app("ls_billingsvr", "src/ls_billingsvr", function()
    multiprocessorcompile "Off"
    files { "src/ls_billingsvr/Util/cJSON.c", "src/ls_billingsvr/crt_compat.cpp" }
    removefiles { "src/ls_billingsvr/BillingRelayServer.cpp", "src/ls_billingsvr/Channeling/ioChannelingNodeNexonSession.cpp", "src/ls_billingsvr/Local/ioLocalSA.cpp" }
    includedirs { "ThirdParty/OpenSSL", "ThirdParty/LS_HTTP", "ThirdParty/LS_NXSoap", "ThirdParty/LS_RestAPI", "ThirdParty/LS_GoogleDump", "ThirdParty/GAuthClientDLL" }
    libdirs { "lib/OpenSSL" }
    links { "LS_GoogleDump", "tinyxml", "LS_HTTP", "LS_NXSoap", "LS_RestAPI", "legacy_stdio_definitions" }
    filter "configurations:Debug"
        links { "GAuthClientDLL", "libcurld" }
    filter "configurations:Release"
        links { "GAuthClientDLL", "libcurl" }
        defines { "VALOFE_NEW_BILLING_SYS_SYH" }
    filter {}
    -- remove default first-party links already added; keep system ones (dbghelp etc already in base)
end)

-- ls_dbagent (no PCH, #import needs /MP off)
ls_app("ls_dbagent", "src/ls_dbagent", function()
    multiprocessorcompile "Off"
    removefiles { "src/ls_dbagent/MainFrm.cpp", "src/ls_dbagent/INI/ioDataChunk.cpp", "src/ls_dbagent/INI/ioINILoader.cpp", "src/ls_dbagent/INI/ioINIParser.cpp", "src/ls_dbagent/INI/ioStream.cpp" }
end)

-- ls_filewritesvr (PCH, NMCrypt, gdiplus, LS_GoogleDump)
ls_app("ls_filewritesvr", "src/ls_filewritesvr", function()
    pchheader "stdafx.h"; pchsource "src/ls_filewritesvr/stdafx.cpp"
    includedirs { "ThirdParty/NMCrypt", "ThirdParty/LS_GoogleDump" }
    links { "NMCrypt", "wininet", "gdiplus" }
    links { "LS_GoogleDump" }
    filter {}
end)
