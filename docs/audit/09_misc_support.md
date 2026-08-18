# 09 — Misc / Support Subsystems

**Area:** INI loader, word filter, encode/crypto, local/license, channeling, IP blocker, monitoring, helper, hack/packet/process checker, critical error, log manager, Util.
**File dibaca:** `INILoader/*`, `Filter/*`, `Encode/*`, `Local/*`, `Channeling/*`, `IPBlocker/*`, `MonitoringServerNode/*`, `EtcHelpFunc.cpp/.h`, `HackCheck.cpp/.h`, `ioPacketChecker.cpp/.h`, `ioPacketStatistics.cpp/.h`, `ioProcessChecker.cpp/.h`, `ioCriticalError.cpp/.h`, `ioEtcLogManager.cpp/.h`, `Manager.cpp/.h`, `Util/*`, `NodeInfo/LicenseManager.cpp/.h`, `AcceptorMonitorNode.cpp/.h`

---

## CRITICAL

### X-F22 — Monitoring port accept unauth shutdown command
- **Lokasi:** `MonitoringServerNode/MonitoringNode.cpp:201-227` (`OnChange` → `g_App.Shutdown(SHUTDOWN_SAFE,15)` untuk `CHANGE_EXIT`); `CheckNS` return `true` unconditional (`:68-71`)
- **Tipe:** Missing auth admin command → remote DoS
- **Root cause:** Peer TCP mana saja yang complete monitoring handshake bisa kirim `MONITORING_CHANGE_CMD` (0x9003) dgn `m_iReqStatus=CHANGE_EXIT (3)`. **Tidak** ada auth, token, IP allowlist, atau `CheckNS` validation. Proteksi structural tunggal = bind address monitoring listen socket (`ioMonitoringBind` `Network/GameServer.cpp:36-39`); jika bound `0.0.0.0` (atau `INADDR_ANY`) remote attacker connect & shutdown server dgn single ~8-byte packet. Bahkan bound 127.0.0.1, local process mana saja (atau SSRF/relay) bisa.
- **PoC:** `connect(monitor_port); send({cmd=9003, size=8, m_iReqStatus=3});` → server initiate shutdown.
- **Fix:** Require shared-secret token di change packet; restrict acceptor ke Unix socket/named pipe; bind loopback + IP allowlist; log & alert change request.

### X-F15 — License shutdown over UDP dgn hardcoded key + spoofable IP check
- **Lokasi:** `NodeInfo/LicenseManager.h:4-6` (`SHUTDOWN_KEY`, `LICENSE_SERVERIP`, `LICENSE_SERVERPORT`); `LicenseManager.cpp:28-61` (`OnLicense`); `MainProcess.cpp:1411-1415` (`LUPK_SHUTDOWN` dispatch)
- **Tipe:** Remote unauth shutdown via UDP spoof + hardcoded key
- **Root cause:** UDP `GUPK_LICENSE` packet dgn `szShutDownKey == SHUTDOWN_KEY` (hardcoded constant binary) & source IP == `LICENSE_SERVERIP` trigger `g_App.Shutdown(SHUTDOWN_SAFE, 20)`. Kedua proteksi bypassable:
  1. `SHUTDOWN_KEY` = string constant embed binary — extractable siapa saja dgn binary client/server.
  2. Sender-IP check pakai UDP source address (`sender_addr`) — spoofable untuk one-way command (tidak butuh handshake/response).
  On-path attacker, atau siapa saja yang spoof IP license server, kirim single UDP packet & shutdown server.
- **PoC:** Forge UDP `GUPK_LICENSE` ke port UDP game dgn `dwMin=1, dwMax=2, szShutDownKey=SHUTDOWN_KEY`, source IP = `LICENSE_SERVERIP` → server shutdown dalam menit.
- **Fix:** Require crypto-signed message (HMAC per-server secret) over nonce + timestamp; jangan trust UDP source IP alone; rate-limit & log.

### X-F31 — Stack buffer overflow di IP parser, reachable dgn relay-controlled data
- **Lokasi:** `EtcHelpFunc.cpp:613-631` (`GetStringIPToDWORDIP`)
- **Tipe:** Stack buffer overflow
- **Root cause:** `char szCut_ip[4][4];` (16 byte). Loop write `szCut_ip[cut_ip][count++] = szIP[i]` **tanpa** bound `count` (per-octet digit count) & tanpa bound `cut_ip` (number octet). String dgn octet >3 char, atau >4 octet, write past `szCut_ip`. Function dipanggil dari `User.cpp:7151` di `m_public_ip`, yang set dari relay packet — `ServerNode::OnChangeAddress` (`ServerNode.cpp:7008-7032`) `char szIpaddr[STR_IP_MAX]; kPacket >> szIpaddr;` lalu `SetClientAddressForRelay(szIpaddr,...)` → `strcpy_s(m_public_ip, szIpaddr)`. `>>` ke `char[64]` itu sendiri overflowable (X-F34), & bahkan IP string 64-byte dgn group >3-digit overflow `szCut_ip` di sini.
- **PoC:** Relay server (atau attacker inject relay stream) kirim `OnChangeAddress` dgn `szIpaddr = "1.2.3.4444..."` (4+ digit di 1 octet) → `count` exceed 3 → write past `szCut_ip[cut_ip][3]` → stack smash → RCE potential.
- **Fix:** Validate `count < 3 && cut_ip < 4`; reject non-numeric; pakai `inet_pton`/`inet_addr` bukan hand-rolled parse.

### X-F34 — `SP2Packet::operator>>(LPTSTR)` copy packet-length string ke caller buffer unknown size
- **Lokasi:** `Network/SP2Packet.cpp:548-562`
- **Tipe:** Buffer overflow (generic engine)
- **Root cause:** `int nlen = lstrlen((LPTSTR)&m_pBuffer[m_currentPos]) * sizeof(TCHAR) + sizeof(TCHAR);` lalu `memcpy(arg, &m_pBuffer[m_currentPos], nlen);` — destination `arg` size unknown ke function; hanya check *source* packet punya `nlen` byte (`CheckRightPacketSize`). Caller `>>` ke `char[N]` fixed overflowed oleh packet string longer than N (dan tidak NUL-terminated dalam N). Safe variant `Read(LPTSTR arg, int size)` (`:1149`) ada tapi `operator>>` tidak pakai. Contoh: `ServerNode::OnChangeAddress` (`char szIpaddr[STR_IP_MAX]; kPacket >> szIpaddr;` — X-F31), & puluhan `kPacket >> szFixedBuf` di `NodeInfo/*` (ID/password/IP field size `ID_NUM_PLUS_ONE`=21, `STR_IP_MAX`=64, dll).
- **PoC:** Path paket apa pun yang `char buf[N]; kPacket >> buf;` dgn attacker control packet body: kirim string longer than N tanpa interior NUL → overflow `buf`. X-F31 = confirmed relay-reachable instance.
- **Fix:** Hapus `operator>>(LPTSTR)`; paksa caller pakai `Read(LPTSTR arg, int size)` atau read ke `ioHashString`.

---

## HIGH

### X-F07 — SEED "encryption" hardcoded IV + shared key — forgeable
- **Lokasi:** `Util/ioEncrypted.cpp:14-18` (IV table hardcoded binary), key `g_App.GetSecondKey()` (shared secret config/binary)
- **Tipe:** Broken crypto / auth bypass
- **Root cause:** SEED-CFB dgn constant IV baked di binary & key = single shared secret stored server. Siapa saja ekstrak binary/config bisa encrypt/decrypt login key arbitrary. `ioLocalKorea::IsRightLoginKey` (`ioLocalKorea.cpp:105-111`) hanya compare `strcmp(szDBKey, szDecryptKey)==0`, jadi forge login key dgn known key+IV lewat validation. Foundation banyak downstream auth-bypass chain.
- **PoC:** Ekstrak `SecondKey` & Korea IV dari binary, craft `ENC_LOGIN_KEY_NUM`-byte hex login key yang SEED-decryption = DB-stored key, kirim login → `IsRightLoginKey` return true.
- **Fix:** Per-session nonce + HMAC over TLS-secured channel; jangan compare decrypted secret cleartext.

### X-F26 — Anti-cheat "quiz" trivially bypassable: client compute answer sendiri
- **Lokasi:** `HackCheck.cpp:129-203` (`GenerateProblem` + `SolveProblem`)
- **Tipe:** Anti-cheat bypass
- **Root cause:** Server generate trivial arithmetic problem (a op b), kirim operand + operator ke client, client return answer. Macro/cheat tool compute answer programmatically — tidak ada proof-of-work, server-side timing integrity, client attestation. "Macro/abuse quiz" tidak bisa detect macro yang arithmetic basic.
- **PoC:** Bot intercept quiz packet, parse `a, op, b`, compute `a+b` (dll), return dalam time limit → selalu lulus.
- **Fix:** Real anti-cheat (server-side simulation cross-check, replay detection, signed client telemetry) bukan arithmetic yang client solve.

### X-F33 — `GetSafeTextWriteDB` (SQL-escape) incomplete
- **Lokasi:** `EtcHelpFunc.cpp:535-568`
- **Tipe:** SQL injection (incomplete escape) + string corruption
- **Root cause:** Escape hanya `'`→`''`, `<`→`&lt;`, `>`→`&gt;`, `\n`→`<br>`. **Tidak** escape `\\` (backslash), `"` (double quote), NUL (0x00), `\r`. Di DB stack yang interpret `\` sebagai escape (MySQL default `NO_BACKSLASH_ESCAPES` off), `\` sebelum injected `'` netralisasi `''` doubling (`\''` → escaped quote + literal quote → injection). NUL truncate string di C-string DB API. `\r` cause log/display injection. Tambahan DBCS lead-byte logic flip `bFirstLeadByte` tapi **masih proses second byte** lewat escape branch (`:552` `if(!bFirstLeadByte)` true di trail-byte iteration), jadi trail byte sama `'`/`<`/`>`/`\n` wrongly escaped, corrupt valid multi-byte text (bisa break 2-byte char jadi lone lead byte → cascade).
- **PoC:** Set nickname `\' OR 1=1--` → jika DB layer tidak re-escape backslash, `GetSafeTextWriteDB` produce `\'' OR 1=1--` & DB lihat escaped quote + unescaped `'` → SQL injection di query built dari output ini.
- **Fix:** Pakai DB client parameterized API / `mysql_real_escape_string` equivalent; escape `\\`, `\"`, `\0`, `\r`; jangan escape byte dalam multi-byte sequence (parse codepage proper atau operasi wide string).

### X-F32 — `IsStringCheck`/`SplitString` stack overflow di long token (SUSPECTED)
- **Lokasi:** `EtcHelpFunc.cpp:782-812` (`IsStringCheck`), `:814-835` (`SplitString`)
- **Tipe:** Stack buffer overflow
- **Root cause:** `char szDst[MAX_PATH]` (260) diisi `szDst[iDstCount++] = szFullText[i]` sampai section char hit — no bound `iDstCount`. Token longer than 260 byte smash `szDst`. Caller: `DungeonAMode.cpp:193`, `HeadquartersMode.cpp:138`, `ioMonsterMapLoadMgr.cpp:516`, `MonsterSurvivalMode.cpp:250`, `RaidMode.cpp:397`, `TowerDefMode.cpp:488` — pass `szBuf` dari data table/INI. Jika `szBuf` dari packet atau tampered data file → overflow.
- **PoC:** Craft data line dgn single token >260 char (no `.` separator) → stack overflow saat parse.
- **Fix:** Bound `iDstCount < MAX_PATH-1` & NUL-terminate; pakai `std::string` tokenization.

### X-F14 — `IsRightTimeLoginKey` disabled di `_DEBUG` build
- **Lokasi:** `Local/ioLocalKorea.cpp:74-86` — `#ifdef _DEBUG return true; #endif`
- **Tipe:** Auth bypass / replay-window disabled
- **Root cause:** Debug build 30-minute login-key replay window disabled; captured login key accepted forever. Jika debug build deploy (atau macro accidentally defined) → login replay attack succeed indefinite.
- **Fix:** Enforce replay window di semua build; gate debug-only behavior di explicit run-time flag, bukan `_DEBUG`.

---

## MEDIUM

### X-F05 — "Word filter" hanya block `\n` — no profanity/chat filtering
- **Lokasi:** `Filter/WordFilterManager.cpp:24-27` (`InsertBlockWords` push only `'\n'`); caller `NodeInfo/ioEtcItem.cpp:1224` & `NodeInfo/User.cpp:16900`
- **Root cause:** `m_vBlockWords` berisi tepat 1 entry `\n`. `CheckSpecialLetters` return FALSE hanya jika candidate contain `\n`. Invoked **hanya** di nickname-change path (change-ID item, first-ID change), **tidak pernah** di chat. Jadi:
  1. Chat completely unfiltered — profanity/abuse/URL/PII bebas broadcast.
  2. Nickname bisa contain `\r`, tab, control char (kecuali `\n`), leading/trailing space, mixed-script homoglyph (mis. `bad\u000Dword`).
- **PoC:** Kirim chat packet profanity apa pun → broadcast verbatim. Set nickname `"Admin\r"` (CR allowed) → log/display injection.
- **Fix:** Implement actual word/regex filter list dari config & apply ke semua chat + nickname path; normalize unicode; strip control char.

### X-F09 — `Decode15` tidak null-terminate output & ignore binary content
- **Lokasi:** `Util/ioEncrypted.cpp:157` — `strncpy(szPlain, (char*)Plain, decodeLen);`
- **Root cause:** `strncpy` tidak null-terminate saat `decodeLen` = buffer capacity. `Plain` mungkin berisi non-text byte dari CFB decryption; downstream `strlen`/`strcmp` di `szPlain` (mis. `IsRightLoginKey`) read past buffer sampai NUL coincidental.
- **Fix:** Selalu `szPlain[decodeLen] = '\0'` setelah `strncpy`; ensure caller buffer `decodeLen+1` byte.

### X-F13 — License expiration check vs local system clock & never expires
- **Lokasi:** `Local/ioLocalParent.cpp:13-23` (`IsRightLicense` pakai `GetLocalTime`); `Local/ioLocalKorea.cpp:188-191` return `30111131` (year 3011)
- **Root cause:** `GetLocalTime()` baca host wall clock; operator/attacker yang set server clock backward bypass expiration. `ioLocalKorea::GetLicenseDate()` return 30111131 — effectively "never expires" — jadi `iDate >= GetLicenseDate()` selalu false & check no-op. Locale lain juga return far-future date.
- **PoC:** Set Windows clock ke 2010 → `IsRightLicense()` return true forever; atau rely far-future date.
- **Fix:** Monotonic trusted time source (NTP-anchored, signed) & store real expiration; refuse run if clock jump backward.

### X-F16 — `LicenseManager::OnLicense` divide-by-zero saat `dwMinShutDownMinutes == dwMaxShutDownMinutes`
- **Lokasi:** `NodeInfo/LicenseManager.cpp:43-56` — `dwGapTime = dwMax - dwMin; ... rand()%dwGapTime;`
- **Root cause:** Hanya `dwMin==0 || dwMax==0` reject. Jika `dwMin==dwMax` (both nonzero), `dwGapTime=0` & `rand()%0` UB (x86 `idiv` → divide-by-zero exception → process crash).
- **Fix:** Reject `dwMax <= dwMin`.

### X-F17 — `ioIPBlocker::Load` leak file buffer di setiap return path
- **Lokasi:** `IPBlocker/ioIPBlocker.cpp:25-40`
- **Root cause:** `BYTE *buffer = new BYTE[length+1];` lalu 3 early-return path (`file.Read` fail, `Tokenize` fail, success) tidak `delete[] buffer`. `std::string text = reinterpret_cast<char*>(buffer);` copy data tapi original `buffer` orphan. Tiap `Load` (black+white list) leak `length+1` byte; reload amplify.
- **Fix:** `std::vector<BYTE>` atau wrap `std::unique_ptr`; atau `delete[] buffer` sebelum setiap return.

### X-F19 — `ioIP` subnet range off-by-one (`.255` dari /24 bypass block)
- **Lokasi:** `IPBlocker/ioIP.cpp:104-125` — `m_IPex[i] = m_IP[i] + 254;`
- **Root cause:** Untuk entry `/n`, octet beyond subnet mask di-set `m_IP[i] + 254`, bukan `+ 255`. `/24` blacklist `192.168.1.0/24` → range `192.168.1.0 .. 192.168.1.254`, jadi `192.168.1.255` **tidak** match & bypass blacklist (inverse untuk whitelist). `int` octet compare tanpa `>255` sanity → malformed entry produce nonsense range.
- **PoC:** Blacklist `10.0.0.0/24`; connect dari `10.0.0.255` → `Find()` return FALSE, tidak blocked.
- **Fix:** Pakai `+ 255` & mask benar; atau compare sebagai 32-bit int dgn netmask.

### X-F20 — `IPBlockerManager` tidak ada per-IP connection limit, dynamic blocking, UDP-source validation
- **Lokasi:** `IPBlocker/IPBlockerManager.cpp` (whole), `ioIPBlocker::Find` linear O(n) scan
- **Root cause:** Hanya static black/white list load dari file startup; tidak ada per-IP connection-count threshold, tidak ada dynamic add. `Find` O(n) linear ke seluruh list tiap connection. Attacker rotate IP (botnet/VPN) tidak pernah throttled, & list besar slow tiap accept.
- **Fix:** Hash-map lookup, per-IP connection counter, dynamic blocking on threshold.

### X-F27 — `HackCheck::GenerateProblem` pakai `rand()` — predictable
- **Lokasi:** `HackCheck.cpp:141,162,177` (`rand() % ...`)
- **Root cause:** `rand()` unseeded-by-this-code (global, likely `srand(time())`) & tidak CSPRNG. Attacker observe beberapa quiz output bisa predict operand/operator berikut & pre-answer.
- **Fix:** CSPRNG (`BCryptGenRandom`).

### X-F36 — `CallURL` SSRF primitive jika dipanggil dgn attacker-controlled URL (SUSPECTED)
- **Lokasi:** `EtcHelpFunc.cpp:665-680`
- **Root cause:** `InternetOpenUrl(hSession, szCallURL, ...)` buat server fetch arbitrary URL no allowlist, no scheme restriction, no redirect cap. Jika caller pass user content (mis. guild-mark URL dari `s_szGuildMarkBlockURL` config — operator-controlled, lower risk) → server probe internal service.
- **Fix:** Restrict scheme `https`, allowlist host, disable redirect, set timeout; jangan pass user input.

### X-F43 — `cSerialize::GetString` read attacker-controlled `length` & copy sebelum validate source bounds
- **Lokasi:** `Util/cSerialize.h:152-176`
- **Root cause:** `GetStringLength` read `uint16 length` dari packet. `GetString` lalu `if(length < size) CopyMemory(buffer, m_temporary+m_offset, length);` & hanya **setelah** check `m_offset <= m_maxLength`. Check `m_offset + length <= m_maxLength` missing *sebelum* copy → crafted `length` larger than remaining packet byte read past packet buffer (info leak / crash). Dest protected `length < size`, source tidak.
- **Fix:** Validate `m_offset + length <= m_maxLength` sebelum `CopyMemory`.

---

## LOW

### X-F01 — INI loader memory leak duplicate section
- **Lokasi:** `INILoader/ioINIParser.cpp:83-94`
- `pKeyList = new KeyList; m_TitleList.insert(...)`. `std::map::insert` tidak overwrite; section dup → second alloc tidak delete/store. Leak 1 `KeyList` per dup tiap parse.
- **Fix:** Check `insert` returned `pair.second`; on fail `delete pKeyList`.

### X-F02 — `ParseKey` unsigned-loop OOB read / exception
- **Lokasi:** `ioINIParser.cpp:107-118`
- `string::size_type i` unsigned; `for(i=tSize-1; i>=0; i--)` — `i>=0` always true. Exit hanya `break`. Jika key line semua trailing whitespace (no non-ws) → `szKeyName` empty, `i` underflow `SIZE_MAX`, `vecparams[0].at(i)` throw `out_of_range` → uncaught → `terminate`. INI server-side → remote DoS unlikely unless `DoMemoryParsingFromMemory` remote (none found).
- **Fix:** Signed type atau `while(i-- > 0)` dgn explicit empty check; reject empty key earlier.

### X-F03 — `ioINILoader` constructor `GetCurrentDirectory` + `StringCbCat` no length validation
- **Lokasi:** `INILoader/ioINILoader.cpp:21-23`, `SetFileName` (`:52-60`), `SetTitle` (`:63-66`)
- `m_szFileName[MAX_PATH]` filled `GetCurrentDirectory(MAX_PATH,...)` lalu 2 `StringCbCat`. Jika CWD + filename > MAX_PATH → `StringCbCat` silently truncate. Wrong/truncated path opened silently.
- **Fix:** Validate `HRESULT` setiap `StringCb*`; fail loudly on truncation.

### X-F04 — `LoadFloat`/`LoadInt`/`LoadBool` `atoi`/`atof` unvalidated content
- **Lokasi:** `ioINILoader.cpp:150-156`, `:102`, `:125`
- `atoi`/`atof` return 0/undefined untuk malformed; caller (mis. `EtcHelpFunc.cpp:155 iMaxSize = LoadInt("MaxSize",0)` lalu `for i<iMaxSize`) trust value. Negative/huge flow unchecked.
- **Fix:** Wrap `LoadInt` dgn range validation di call site.

### X-F06 — Filter apply nickname only; nickname `\r` enable log injection
- **Lokasi:** `WordFilterManager.cpp:42-53` (only `\n` blocked)
- `\r` allowed; `LOG.PrintTimeAndLog("%s ... szNewPublicID.c_str() ...")` emit CR → forge log entry / break log parser.
- **Fix:** Strip `\r` & control char di filter.

### X-F08 — `Encode15` lie about destination buffer size ke `strcpy_s`
- **Lokasi:** `Util/ioEncrypted.cpp:82` — `strcpy_s(szCipher, sizeof(szTemp), szTemp);`
- `szCipher` = caller buffer unknown size; function pass `sizeof(szTemp)` (260, *local* buffer) sebagai dest size. `strcpy_s` trust itu. Hex output ≤31 byte → overflow hanya jika caller buffer < hex length. API contract broken & brittle.
- **Fix:** Take `destSize` param & pass ke `strcpy_s`; atau return `std::string`.

### X-F10 — `Decode15` hex-parse read past packet jika `szCipher` tidak NUL-terminated/odd length
- **Lokasi:** `Util/ioEncrypted.cpp:109-117` — `memcpy(szTempOneHex, &szCipher[pos], 2)` loop bound `decodeLen = strlen(szCipher)/2`
- Jika `szCipher` length odd, `decodeLen` truncate & last `memcpy` read `szCipher[pos]` + `szCipher[pos+1]` (pos+1 = NUL, harmless) — tapi tidak validate hex char; `strtol` tolerate garbage → arbitrary `Cipher[i]` → decrypt arbitrary plaintext yang compare sebagai login key (X-F07).
- **Fix:** Validate hex char & even length sebelum loop.

### X-F11 — MD5 context wipe pakai `sizeof(ctx)` (pointer size), bukan struct size
- **Lokasi:** `Util/md5.cpp:149` — `memset(ctx, 0, sizeof(ctx));`
- `ctx` = `MD5Context*`; `sizeof(ctx)` = 4/8 byte. Hanya pointer's first byte zeroed, **bukan** digest state — sensitive state remain di stack. MD5 juga crypto-broken. "Checksum"/"integrity" use MD5 = forgeable.
- **Fix:** `memset(ctx, 0, sizeof(*ctx))`; ganti HMAC-SHA256 untuk integrity.

### X-F12 — `SEED_*Update`/`Final` `BufLen` masking bug `(AlgInfo->BufLen & 0xF0000000)`
- **Lokasi:** `Encode/seedenc.cpp:310,367,593,658,714,772`
- `AlgInfo->BufLen = (AlgInfo->BufLen&0xF0000000) + PlainTxtLen;` — mask `&0xF0000000` hampir selalu 0, accidentally work small input tapi wrong. Large/multi-call stream → corrupt chain.
- **Fix:** Pakai reference SEED implementation; hapus mask.

### X-F18 — `ioIP::SetIP`/`operator==` pakai `static TOKENS` — not thread-safe
- **Lokasi:** `IPBlocker/ioIP.cpp:49-63` (`operator==(string)`) & `:84-132` (`SetIP`) keduanya `static TOKENS tokens;`
- Single static `vector<int>` reused semua `ioIP` instance & thread via `.clear()` + `Tokenize`. Concurrent corrupt vector.
- **Fix:** Local variable.

### X-F21 — `ioIPBlocker::Tokenize` push empty token untuk trailing delimiter
- **Lokasi:** `IPBlocker/ioIPBlocker.cpp:75-94`
- File end `\r\n` → loop append empty token → `ioIP("")` dgn `m_IP` all zero; harmless tapi pollute list & `IsActive()` true untuk empty file effectively.
- **Fix:** Skip empty token.

### X-F23 — `MonitoringNode::OnChange` `CHANGE_OPEN`/`CHANGE_BLOCK` no-op (silent failure) tapi `CHANGE_EXIT` honored
- **Lokasi:** `MonitoringServerNode/MonitoringNode.cpp:208-222`
- Open/block always return `CHANGE_FAIL` no state change (harmless), tapi lull operator percaya monitoring read-only sementara `CHANGE_EXIT` live & unauth (X-F22).
- **Fix:** Hapus `CHANGE_EXIT` atau protect (X-F22).

### X-F24 — `MonitoringNodeManager` tidak free pooled `MonitoringNode` kecuali via `ReleaseMemoryPool`
- **Lokasi:** `MonitoringServerNode/MonitoringNodeManager.cpp:53-65,90-102`
- `RemoveNode` return node ke `m_memNode` pool (OK), tapi jika `ReleaseInstance`/`ReleaseMemoryPool` tidak call saat shutdown → pool leak. `CreateNode` return node yang jika `AfterCreate` fail → `SessionClose()` tapi caller `AcceptorMonitorNode::OnAccept` (`:52-60`) masih `AddNode` conditional — minor ownership ambiguity.
- **Fix:** Deterministic teardown; RAII.

### X-F25 — Channeling "not supported" path log `pUser->GetPublicID()` (attacker-controlled) level 0
- **Lokasi:** `Channeling/ioChannelingNodeParent.cpp:149,155,161,167,173,179`
- `LOG.PrintTimeAndLog(0,"[Channeling :%d][%s] %s Not Supported ", pUser->GetChannelingType(), pUser->GetPublicID(), __FUNCTION__);` — public ID attacker-controlled, unescaped; kombinasi X-F06 (`\r` allowed) → log-line forge. Level 0 = always print.
- **Fix:** Sanitize ID sebelum log; proper log level.

### X-F28 — `ioPacketChecker`/`ioEtcLogManager` map grow unbounded by distinct packet ID
- **Lokasi:** `ioPacketChecker.cpp:96-120,122-146`; `ioEtcLogManager.cpp:65-91`
- `m_SessionPacket`/`m_QueryPacket` vector & `m_RoomEnterLoadMap`/`m_UDPPacketID` map hanya clear saat `WriteLOG()`. Jika `WriteLOG` tidak call (`m_dwLogTime==0` `ioProcessChecker`) atau rare → distinct ID accumulate. Packet ID fixed enum → bounded; `m_UDPPacketID` sama. Low; `m_RoomEnterLoadMap` keyed by measured seconds bisa grow jika logging disabled.
- **Fix:** Cap map size; periodically clear regardless log time.

### X-F29 — `ioProcessChecker::WriteLOG` pakai `static char szLOG[2000]` formatted dgn attacker-influenced int count (safe) tapi no thread guard
- **Lokasi:** `ioProcessChecker.cpp:146-198`
- `static` buffer shared across thread (IOCP worker call `ProcessIOCP`); concurrent `WriteLOG` multi-thread corrupt `szLOG`. Content all integer (no string inject).
- **Fix:** Local automatic buffer.

### X-F30 — `ioCriticalError` build command line contain `rkPublicID.c_str()` (attacker-controlled)
- **Lokasi:** `ioCriticalError.cpp:214-219` (`CheckCrashLog`), `:222-233`, `:235-246`, dll — `sprintf_s(szCmd, "...%s...", rkPublicID.c_str(), ...)` lalu `ExcuteProcess("CriticalError.exe", szCmd)`
- `ExcuteProcess` fully commented out (`:72-92`), jadi tidak execute hari ini. Jika re-enable, attacker-controlled public ID concat ke command line `CreateProcess`. `CreateProcess` tidak shell-interpret metachar, tapi `CriticalError.exe` arg parsing mungkin. `sprintf_s` bound `szCmd[2048]` → no overflow, tapi truncation long ID mangle command.
- **Fix:** Jangan embed raw user input ke command line; pass via temp file/env var; validate ID length/charset sebelum format.

### X-F35 — `GetLocalIpAddressList` mutate static `hostent` return `gethostbyname`
- **Lokasi:** `EtcHelpFunc.cpp:1081-1094` — `lpstHostent->h_addr_list++;`
- Increment `h_addr_list` pointer di static buffer `gethostbyname` → corrupt untuk `gethostbyname` user berikut di process.
- **Fix:** Index `h_addr_list[i]` bukan increment.

### X-F37 — `ioStringConverter::toString` pakai `static m_ConvertBuf[MAX_PATH]`
- **Lokasi:** `Util/ioStringConverter.cpp:9,11-44`
- Single shared buffer semua `toString` call across thread; concurrent read partial output other. Return `ioHashString` copy → window kecil tapi real.
- **Fix:** Local atau `thread_local`.

### X-F38 — Log file name built dari `szIP.c_str()`/`GetLogFolder()` tanpa sanitization
- **Lokasi:** `MainProcess.cpp:1775-1877` — `StringCbPrintf(TimeLogName, ..., "%s\\BUG%s-%s-%d.log", GetLogFolder(), szCurTime, szIP.c_str(), iCSPort)`; `ioCriticalError.cpp:66` — `sprintf_s(szFile, "%s/critical.log", g_App.GetLogFolder())`
- Input dari server config `m_szLogFolder` via `GetPrivateProfileString` & server own IP, bukan langsung dari client packet. Attacker yang bisa write `config.ini` `Log` key (mis. via config-injection bug lain) bisa set `..\\..\\..\\Windows\\System32\\` → server create log file luar log dir. `StringCbPrintf` bound dest `TimeLogName[MAX_PATH]` → no overflow, tapi path traversal dalam MAX_PATH work.
- **Fix:** Canonicalize & validate log folder startup; reject `..` & absolute path dari INI value.

### X-F39 — `ioEtcLogManager`/`ioProcessChecker`/`ioPacketChecker` log attacker-controlled ID via `%s` int (safe) — tapi `ioCriticalError` log `rkPublicID.c_str()`
- **Lokasi:** `ioCriticalError.cpp:227,240,254,269,282,295,308,321,334,344,357,368,371`
- `CriticalLOG.PrintTimeAndLog(LOG_DEBUG_LEVEL, "%s - Soldier Max Count : %d", rkPublicID.c_str(), ...)` — public ID attacker-controlled raw; dgn `\r` allowed (X-F06) forge log line. `critical.log` juga di-delete saat init (`ioCriticalError.cpp:66-67`) → attacker yang trigger repeated restart keep wipe audit log.
- **Fix:** Sanitize ID for log; jangan delete critical log tiap start (append/rotate).

### X-F40 — `ioCriticalError::Initialize` delete `critical.log` saat startup
- **Lokasi:** `ioCriticalError.cpp:65-67`
- `::DeleteFile(szFile)` wipe previous critical log tiap server start → destroy forensic evidence prior crash/abuse.
- **Fix:** Rename ke `critical.<timestamp>.log` bukan delete.

### X-F41 — Manual singleton leak jika `ReleaseInstance` tidak call
- **Lokasi:** `ioPacketChecker.cpp:27-30`, `ioProcessChecker.cpp:53-56`, `ioCriticalError.cpp:41-44`, `ioEtcLogManager.cpp:27-30`, `MonitoringNodeManager.cpp:26-29`
- Store `sg_Instance = new ...` & free hanya via explicit `ReleaseInstance()`. Jika shutdown skip (mis. `g_App.Shutdown(SHUTDOWN_QUICK,25)` `Manager.cpp:91`) → singleton + member leak. Windows OS reclaim at exit → negligible impact kecuali leak-detector noise / dirty-shutdown diagnostic.
- **Fix:** `std::unique_ptr` static atau `Singleton` auto-destruction.

### X-F42 — `ioHashString` default ctor uninitialized `m_dwHashCode` jika `ReAllocCapacity` skip — & `CalcHashCode(NULL)` risk
- **Lokasi:** `Util/ioHashString.cpp:4-12` (default ctor alloc, OK), `:14-23` (`const char*` ctor: if `str` NULL, `strlen(NULL)` crash)
- `ioHashString(const char* str)` call `strlen(str)` no NULL check; beberapa caller pass `c_str()` empty string (safe) tapi NULL propagation crash. Tidak direct remote vector kecuali packet read yield NULL & feed ke `ioHashString`.
- **Fix:** Guard `if(!str) str = "";` di ctor.

### X-F44 — `cBuffer::Create(0)` alloc `new uint8[0]` & beberapa path skip `if(m_pDT)` check
- **Lokasi:** `Util/cBuffer.cpp:51-67`, & `Append`/`Copy` variant rely `m_pDT`
- `cBuffer::Create(0)` → `new uint8[0]` (legal tapi unusable); `m_max=0` → setiap `Append`/`Copy` silent fail return false & caller jarang check (`cSerialize::operator<<` ignore `Append` bool). Zero-length serialize buffer quietly drop semua write → empty/invalid packet ke client/DB.
- **Fix:** Reject `buffLen==0`; caller check `Append`/`Copy` return.

### X-F45 — `ConsoleHandler` swallow all console event ke `SHUTDOWN_QUICK`
- **Lokasi:** `Manager.cpp:80-95`
- `CTRL_LOGOFF_EVENT`/`CTRL_SHUTDOWN_EVENT` (bahkan `CTRL_C_EVENT`) trigger `SHUTDOWN_QUICK` 25s — terminal-services logoff server killed, potentially tanpa flush DB/log. Bukan security vuln, tapi reliability/DoS untuk operator yang logoff session.
- **Fix:** Distinguish event; hanya `SHUTDOWN_QUICK` real shutdown.

### X-F46 — `Manager::Timer` infinite `while(TRUE)` no visible exit condition
- **Lokasi:** `Manager.cpp:140-160`
- Loop break hanya via `g_App.Shutdown` side effect (presumably `exit()`/`return` dari scheduled command). Jika scheduled shutdown set flag tapi tidak terminate → loop never exit. Bukan direct vuln tapi fragile.
- **Fix:** Check `g_App.IsWantExit()` tiap iterasi.

---

## Prioritas Fix Lokal
1. **X-F22/X-F15 (P0):** Auth HMAC monitoring/license shutdown.
2. **X-F31/X-F34 (P0):** Bounded read IP parser + hapus `operator>>(LPTSTR)`.
3. **X-F07 (P1):** Per-session crypto login key.
4. **X-F33 (P1):** Complete SQL escape.
5. **X-F26 (P3):** Real anti-cheat.
6. **X-F05 (P3):** Real word filter.
7. **X-F19/X-F20 (P2):** IPBlocker fix + per-IP dynamic.
