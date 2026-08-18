# Security Audit — ls_gamesvr (Lost Saga Game Server)

> **Scope:** Analisa menyeluruh vulnerability & memory leak di `src/ls_gamesvr` (di luar third-party anti-cheat: nProtect, Xtrap, XignCode, HackShield).
> **Mode:** Read-only static analysis. Kedalaman: mendalam + skenario PoC.
> **Tanggal:** 2026-08-18
> **Total temuan:** ~110+ (CRITICAL ~18, HIGH ~28, MEDIUM ~35, LOW/INFO ~30+)

---

## Daftar Isi

| # | File | Area |
|---|------|------|
| 00 | (file ini) | Index + ringkasan + konsolidasi |
| 01 | [01_packet_parsing.md](01_packet_parsing.md) | CPacket / SP2Packet |
| 02 | [02_user_parsing.md](02_user_parsing.md) | User / UserParent packet parsing |
| 03 | [03_network_iocp_udp.md](03_network_iocp_udp.md) | Network / IOCP / UDP / Relay |
| 04 | [04_node_managers.md](04_node_managers.md) | Node Managers (memory pool) |
| 05 | [05_db_client.md](05_db_client.md) | DB Client / QueryData |
| 06 | [06_item_inventory_trade.md](06_item_inventory_trade.md) | Item / Inventory / Trade / Ekonomi |
| 07 | [07_battleroom_modes.md](07_battleroom_modes.md) | BattleRoom / Game Modes |
| 08 | [08_manager_singletons.md](08_manager_singletons.md) | Manager Subsystem (singletons) |
| 09 | [09_misc_support.md](09_misc_support.md) | Misc / Support subsystems |
| 10 | [10_interserver.md](10_interserver.md) | Inter-Server (MainServer/BillingRelay/SS) |

---

## Ringkasan Eksekutif

Dua akar masalah memperluas dampak hampir seluruh temuan:

### RC-1: `CPacket::operator>>(LPTSTR)` — Stack Buffer Overflow Engine
- **File:** `iocpSocketDLL/SocketModules/Packet.cpp:291-305`
- Membaca string null-terminated via `lstrlen()` lalu `memcpy` ke buffer tujuan **tanpa tahu ukuran tujuan**. Setiap `packet >> charBuf[N]` = potensi stack buffer overflow. Failure path `arg = NULL` cuma null-kan pointer lokal (tidak amankan).
- **Fix:** Hapus operator; paksa `Read(buf, sizeof(buf))` atau `ioHashString`.

### RC-2: `CheckRightPacketSize` cek ke `MAX_BUFFER`, bukan ukuran paket
- **File:** `Packet.cpp:118-127`
- Cek `m_currentPos + iAddSize >= MAX_BUFFER` (65536) bukan `GetBufferSize()`. Pembacaan bisa lewati data paket aktual ke buffer zeroed/stale → OOB read & info leak.

### RC-3: Link antar-server tanpa autentikasi/integritas
- `MainServerNode.cpp:208`, `BillingRelayServer.cpp:135`, `ServerNode.cpp:1000`, `DBClient.cpp:67` — semua `CheckNS` return `true`.
- Crypto keyless: `Encode/cryption.cpp` (XOR 0xFF + rotate), `Util/md5.cpp` (MD5 bukan MAC), checksum `ioUDPSecurity`/`ioServerSecurity` = MD5-XOR keyless → semua forgeable dari binary.
- **Fix:** HMAC per-session + nonce + sequence + TLS di semua link.

---

## Tabel Konsolidasi Semua Temuan (urut severity)

### CRITICAL
| ID | Lokasi | Ringkas |
|----|--------|---------|
| P-1 / RC-1 | `Packet.cpp:291-305` | `operator>>(LPTSTR)` stack BOF engine |
| U-1 / N-1 | `MainProcess.cpp:1401-1402` | UDP pre-auth stack BOF → RCE |
| N-2 | `MainProcess.cpp:1518-1555` | UDP relay SSRF/reflection DoS |
| N-3 | `ioBroadCastRelayModule.cpp:149-164` | `CUPK_TEST` reflection unconditional |
| G-1 | `Mode.cpp:5747` | Award manipulation client-supplied |
| G-2 | `Mode.cpp:5476` | Tournament win forgery |
| G-3 | `Mode.cpp:5375` | Play record forge |
| G-4 | `Mode.cpp:4355` | Damage list forge |
| G-5 | `Room.cpp:2949,2968` | Kill injection arbitrary victim |
| G-6 | `CatchMode.cpp:849` | Prisoner state injection |
| G-7 | `Mode.cpp:3955` | Item spawn by client code |
| S-C1 | `MainProcess.cpp:245-248` | All managers leaked on init/shutdown |
| S-C2 / IS-I5 | `MainServerNode.cpp:1072-1085` | Stack BOF admin event insert |
| X-F22 | `MonitoringNode.cpp:201-227` | Unauth monitoring shutdown |
| X-F15 | `LicenseManager.cpp:28-61` | Unauth UDP shutdown hardcoded key |
| X-F31 | `EtcHelpFunc.cpp:613-631` | Stack BOF IP parser (relay-reachable) |
| X-F34 | `SP2Packet.cpp:548-562` | `>>char[]` BOF (enable banyak) |
| IS-A1..A4 | `MainServerNode.cpp:208`, `BillingRelayServer.cpp:135`, `ServerNode.cpp:1000`, `DBClient.cpp:67` | All inter-server links no auth |
| IS-E1 | `ServerNode.cpp:6802,6823` | Present injection via SS link |
| IS-E2 | `MainServerNode.cpp:914-1070` | Admin item insert unauth |

### HIGH
| ID | Lokasi | Ringkas |
|----|--------|---------|
| U-2 | `User.cpp:2071` + `ioPacketQueue.cpp:57` | `User::CheckNS` tidak pernah dipanggil di dispatch TCP |
| N-0 | `cryption.cpp`, `ioUDPSecurity.cpp` | Crypto forgeable (XOR+MD5 keyless) |
| N-4 | `MainProcess.cpp:1527-1540` | Cross-user packet injection by `dwUserIndex` |
| N-5 | `ioUDPSecurity.cpp:32-107` | FSM magic-token -1 selalu accept; replay check bypass ganti port |
| N-8 | `NetworkSecurity.cpp:16-19` | No UDP rate limiting |
| Mgr-1 | `UserNodeManager.cpp:145-154` | Pool exhaustion pre-login zombie |
| Mgr-2 | `UserNodeManager.cpp:1411-1465` | `m_UserSyncDataVec` stale `User*` UAF |
| DB-1 | `SP2Packet.cpp:678-691` | `>>CQueryResultData` bad bounds + bad_alloc DoS |
| DB-2 | `UserNodeManager.cpp:2270+` | Trusted DB result → logic corruption |
| DB-4 | `DBClient.cpp:4216,4721,5413,5934` | SQL injection (escape tidak konsisten) |
| E-1 | `User.cpp:17980`, `ioEtcItem.cpp:200` | ETC-item use non-atomic dupe |
| E-2 | `User.cpp:35911`, `ioUserAccessory.cpp:448` | Accessory compose same-material exploit |
| E-3 | `User.cpp:4549` | `AddMoney` 10M wipe |
| E-4 | `User.cpp:29462` | Trade register non-atomic item loss |
| E-6 | `ioItemCompoundManager.cpp:426` | Compound target==victim slot destroy |
| E-8 | `ioUserAccessory.cpp:488` | Accessory reinforce base==material loss |
| G-H1 | `BattleRoomNode.cpp:3469` | `BATTLEROOM_READY_GO` bukan owner |
| G-H2 | `BattleRoomNode.cpp:1877` | Unbounded team/observer capacity |
| G-H3 | `Mode.cpp:6092`, `Room.cpp:2907` | Observer bypass all gameplay handlers |
| G-H5 | `Mode.cpp:4241,4327` | Unbounded push-struct list OOM |
| G-H6 | `Room.cpp:3000` | OnPassage raw relay injection |
| G-L6 | `SP2Packet.cpp:619` | `lstrlen` OOB di semua handler baca nama |
| S-H1 | `ioOakBarrelManager.cpp:122,175` | `m_dwRewardRandomMax[12]` OOB INI |
| S-H2 | `ioRandomBoxManager.cpp:28-71` | Reload leak pointer tree |
| S-H3 | `ioPowerUpManager.cpp:396-406` | `vPowerUPCode[-1]` OOB read |
| S-H4 | `MainProcess.cpp:848`, `IORandom.cpp:86` | RNG predictable `srand(timeGetTime)` |
| X-F07 | `ioEncrypted.cpp:14-18` | SEED login-key forgeable (hardcoded IV+key) |
| X-F26 | `HackCheck.cpp:129-203` | Anti-cheat quiz bypassable client |
| X-F33 | `EtcHelpFunc.cpp:535-568` | SQL escape incomplete (\\, ", NUL, \r) |
| X-F32 | `EtcHelpFunc.cpp:782-812` | `IsStringCheck/SplitString` stack BOF |
| X-F14 | `ioLocalKorea.cpp:74-86` | Login-key replay window disabled di `_DEBUG` |
| IS-B1 | `MainServerNode.cpp:555-571` | Unauth shutdown QUICK/SAFE EXIT |
| IS-B2 | `MainServerNode.cpp:1447-1461` | Unauth shutdown LOW_CONNECT_EXIT |
| IS-C1 | `BillingRelayServer.cpp:328-391`, `User.cpp:4628-4636` | Forged cash grant no bounds |
| IS-D1 | `MainServerNode.cpp:1513-1616` | Forged trade-sold drain peso |
| IS-D2 | `ServerNode.cpp:6157,6273,6327,6507` | Forged seller-side trade present grant |
| IS-F1 | `ServerNode.cpp:4823-4866` | Forged guild master change |
| IS-F2 | `ServerNode.cpp:4887-4931` | Forged guild kick |
| IS-F3 | `ServerNode.cpp:4596,4645` | Forged guild create/complete |
| IS-F4 | `ServerNode.cpp:4690-4754` | Forged guild entry agree |
| IS-F6 | `MainServerNode.cpp:1192-1229` | Guild-info grants level + rank reward |
| IS-G1 | `UserCopyNode.cpp:71,116` | Unbounded `iMaxBestFriend` loop OOM |
| IS-G3 | `MainServerNode.cpp:818-871` | `OnAllServerList` topology injection + forced connect |
| IS-G4 | `MainServerNode.cpp:2739-2866` | `OnNodeInfoResponse` redirect DB/Billing endpoint |

### MEDIUM (ringkas, detail di file per area)
U-3, U-4, U-5, U-6, U-7, N-6, N-7, N-12, N-13, N-16, Mgr-3..Mgr-9, DB-3, DB-9, DB-10, DB-11, E-5, E-7, E-9, E-10, E-11, E-12, E-13, E-14, E-19, G-M1..G-M8, S-M1..S-M9, X-F05, X-F09, X-F13, X-F16, X-F17, X-F19, X-F20, X-F27, X-F36, X-F43, IS-C2, IS-D3, IS-D4, IS-F5, IS-G2, IS-H2, IS-I1, IS-I2, IS-I4

### LOW / INFO (ringkas, detail di file per area)
U-8, U-9, U-10, U-11, N-9, N-10, N-11, N-14, N-15, Mgr-10..Mgr-15, DB-6, DB-7, DB-8, DB-12, DB-13, E-15..E-20, G-L1..L5, S-L2..L9, X-F01..F04, F06, F08, F10..F12, F18, F21, F23..F46, IS-D5, IS-F7, IS-H1, IS-H3, IS-H4, IS-I3, IS-I6

---

## Rekomendasi Prioritas Perbaikan

### P0 — Segera (RCE / unauth shutdown / item grant)
1. **Hapus `CPacket::operator>>(LPTSTR)`**; ganti semua `packet >> charBuf` ke `Read(buf, sizeof(buf))` atau `ioHashString` + validasi panjang. Mulai `MainProcess.cpp:1401` & `EtcHelpFunc.cpp:613`.
2. **`MainServerNode.cpp:1072`** `OnAdminEventInsert`: `if(iValueCount > 64) return;`.
3. **Auth semua link inter-server**: HMAC + nonce + sequence + TLS. Sementara: bind SS/monitoring loopback + firewall, hapus `CUPK_TEST`, allowlist peer relay.
4. **Monitoring `CHANGE_EXIT` & License `LUPK_SHUTDOWN`**: wajib token HMAC; jangan trust UDP source IP; hapus `SHUTDOWN_KEY` hardcoded.
5. **Present/admin inject (IS-E1/E2)**: tolak di link unauth; validasi signed token dari main server.
6. **Gameplay record server-authoritative** (G-1..G-7): hitung award/contribute/damage/kill/death di server; tolap `CTPK_AWARDING_RESULT`/`LAST_PLAYRECORD`/`CURRENT_DAMAGELIST`/`DROP_DIE`/`WEAPON_DIE` sebagai sumber data.

### P1 — Ekonomi & integritas
7. **`AddMoney` (User.cpp:4549)**: hapus cutoff 10M, ganti guard hasil overflow; clamp harga trade; tax integer 64-bit.
8. **Non-atomic item/cash**: bungkus consume+reward/persist dalam 1 transaksi DB; refund `RemoveMoney` saat `AddEtcItem` gagal.
9. **Distinct slot validation**: compose/reinforce/compound wajib `iTarget != iVictim`, material pairwise distinct.
10. **`SetCash/AddCash` (User.cpp:4628)**: tambah clamp seperti Money.
11. **SQL escape lengkap** (`GetSafeTextWriteDB`): escape `\\`,`"`,NUL,`\r`; atau parameterized; escape SEMUA field teks.
12. **`CQueryResultData` parse (SP2Packet.cpp:678)**: validasi `nResultBufferSize` vs sisa paket sebelum `new`; reject negative.

### P2 — DoS & memory
13. **UDP rate limit**: implement `UpdateReceiveCount` di `ioUDPSecurity`; per-IP cap; global cap.
14. **TCP connection cap**: per-IP limit di accept; ghost-cleanup tidak skip `SyncTime==0`; timeout pre-login.
15. **`m_UserSyncDataVec` (Mgr-2)**: remove entry saat disconnect / key by index + revalidate.
16. **`MemPooler`**: `Destroy` panggil destructor; `Push` debug duplicate-check; `Get` re-check empty.
17. **`DestroyManagers()`** di `~ioMainProcess` + setiap `return false` LoadINI (S-C1).
18. **Reload manager**: `Init()` sebelum `LoadINI`; `delete` pointer tree sebelum `.clear()` (RandomBox).
19. **Unbounded loop/reserve**: `MAX_GUARD` semua count client-driven.

### P3 — Game logic & hardening
20. **State/role gate di dispatcher**: tolak observer untuk state-mutating packet; wajib `MS_PLAY`; wajib owner untuk control packet.
21. **`OnClose` reorder**: `OnDestroy → OnSessionDestroy → RemoveNode` (Mgr-3).
22. **`CheckNS` di user TCP dispatch (U-2)**: panggil saat `m_bUseSecurity`.
23. **Auth gate pre-login (U-3)**: `IsConnectProcessComplete()` + `GetUserIndex()!=0`.
24. **Anti-cheat (HackCheck)**: ganti quiz dgn server-side simulation + CSPRNG.
25. **Crypto login key (X-F07)**: per-session nonce + HMAC over TLS; hapus hardcoded IV/key.
26. **Word filter**: implement filter chat + nick; strip control char.
27. **IPBlocker**: `+255` bukan `+254`; hash map; per-IP dynamic block.

### P4 — Hygiene
29. Hapus dead SQL string builder; hapus `#if 0` BOF; konsisten `new(std::nothrow)` atau RAII.
30. `static` scratch buffer → stack-local.
31. `MemPooler` threadSafe=TRUE konsisten; assert logic-thread ID.
32. Validasi enum (`TeamType`, `iSubType`, mode/map index) sebelum pakai.
33. `cSerialize::GetString` cek `m_offset+length <= m_maxLength` sebelum copy.
34. `MD5` `memset(ctx,0,sizeof(*ctx))`; ganti HMAC-SHA256 untuk integritas.

---

## Catatan Metodologi

- Analisa berbasis static read-only (baca kode, grep, trace alur). Tidak ada dynamic test.
- Temuan ditandai **SUSPECTED** jika inferensi kuat tapi tidak fully traced ke sink persistence/authority.
- Area di-skip: `nProtect/`, `Xtrap/`, `XignCode/`, `HackShield/` (third-party anti-cheat).
- Library shared `iocpSocketDLL/` & `include/` dianalisa karena dipakai ls_gamesvr (bukan third-party anti-cheat).
- Threading model: IOCP worker (×16) cuma enqueue paket; semua logic di single `LogicThread` → race cross-thread minim, tapi logical UAF/stale-pointer tetap relevan.

---

*Akhir index. Detail tiap area di file 01–10.*
