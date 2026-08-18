# 02 — User Packet Parsing

**Area:** Deserialisasi paket client ke struct user, dispatch handler.
**File dibaca:** `NodeInfo/User.cpp/.h`, `UserParent.cpp/.h`, `UserCopyNode.cpp/.h`, `AcceptorUserNode.cpp/.h`, `MainProcess.cpp`, `ioPacketQueue.cpp`

> **Temuan positif kunci:** Di `User.cpp`, hampir semua baca string pakai `ioHashString` (heap, bounded MAX_BUFFER). `operator>>(LPTSTR)` ke `char[]` **tidak** dipakai di User.cpp. Satu pengecualian katrastofik: **UDP dispatch path di MainProcess.cpp**.

---

## Temuan

### U-1 — CRITICAL: Pre-auth UDP stack buffer overflow di `ProcessUDPPacket`
- **Lokasi:** `MainProcess.cpp:1401-1402`
- **Tipe:** Stack buffer overflow → RCE
- **Root cause:**
  ```cpp
  char szPublicID[ID_NUM_PLUS_ONE] = "";   // 21 byte (Define.h:88) atau 41 (THAILAND)
  rkPacket >> szPublicID;                  // operator>>(LPTSTR) — no dest size
  ```
  `ID_NUM_PLUS_ONE` = 21 (atau 41 THAILAND). `operator>>(LPTSTR)` baca `lstrlen` (scan null dalam 64KB buffer), `memcpy` ke 21-byte stack buffer. UDP datagram bisa bawa ~32768 byte (`g_UDPNode.InitMemory(1000,16384*2,1000)`). String >21 byte (null-terminated dalam datagram) → stack smash `ProcessUDPPacket`.
- **PoC:** Kirim 1 UDP datagram ke port UDP game dgn `PacketID=CUPK_CONNECT` (atau `CUPK_SYNCTIME`/`CUPK_RESERVE_ROOM_JOIN`/`CUPK_CHECK_KING_PING`/`CUPK_CHECK_FLAG_PING`), field pertama string >20 byte non-null-terminated. `lstrlen` ~4000, `CheckRightPacketSize` lolos (4000<65536), `memcpy` tulis 4000 byte ke `szPublicID[21]` → overwrite return address → RCE. **Pre-auth, single UDP, spoofable source IP.**
- **Fix:** Ganti `rkPacket >> szPublicID` jadi `rkPacket.Read(szPublicID, sizeof(szPublicID))` (safe overload `SP2Packet.cpp:1149`), atau read ke `ioHashString` lalu length-validate. Enforce `ID_NUM_PLUS_ONE` sebagai max ketat.

### U-2 — HIGH: `User::CheckNS` (ioServerSecurity) tidak pernah dipanggil
- **Lokasi:** `User.cpp:2071-2102` (definisi), `ioPacketQueue.cpp:45-67` (`ParseSession` dispatch), `User.cpp:8063` (`PacketParsing`)
- **Tipe:** Missing security control / anti-tamper mati
- **Root cause:** `ioPacketQueue::ParseSession` panggil `pSessionNode->PacketParsing(m_SessionPacket)` langsung. `CheckNS` (validasi checksum, FSM state, rate-limit `m_iSecurityOneSecRecv`) **tidak pernah** dipanggil di dispatch TCP user. Satu-satunya call site live `CheckNS` di tree adalah `MonitoringNode.cpp:155` (node beda). `m_pNS`/`ioServerSecurity` dialokasi di `User::OnCreate` saat `m_bUseSecurity` on, tapi cek tidak pernah jalan di paket user.
- **PoC:** Client TCP kirim paket malformed yang gagal checksum/state check, flood di rate berapa pun; `UpdateReceiveCount` rate-limit tak pernah trigger. Kombinasi dgn U-3 → client unauth drive handler sembarang.
- **Fix:** Panggil `if(!CheckNS(packet)) return;` di top `User::PacketParsing` (+ `OnRoomProcessPacket`/`OnBattleRoomProcessPacket`/`OnLadderTeamProcessPacket`) saat `m_bUseSecurity`.

### U-3 — MEDIUM: Tidak ada auth/session-state gate di `User::PacketParsing`
- **Lokasi:** `User.cpp:8063-9157` (switch besar), pre-dispatch `OnRoomProcessPacket`/`OnBattleRoomProcessPacket`/`OnLadderTeamProcessPacket` di `:8079-8086`
- **Tipe:** Missing authorization / state confusion
- **Root cause:** Tidak ada top-level check (`IsConnectProcessComplete()` / `GetUserIndex()!=0`) sebelum switch. Hanya `OnConnect`/`OnMovingServer` untuk pre-login. `CTPK_*` lain diproses walau user belum login (`m_user_data.m_user_idx==0`, `m_public_id` kosong). Contoh: `OnFriendList` (`:9494`) panggil `g_DBClient.OnSelectFriendList(...,GetUserIndex(),GetPublicID(),iCount)` dgn user_idx=0; `OnRegisteredUser` (`:9654`) → `OnSelectUserIDCheck`; `OnUserPosRefresh` (`:9618`).
- **PoC:** Buka TCP socket, kirim `CTPK_FRIEND_LIST_MSG` (dgn `iCount` sembarang) sebelum `CTPK_CONNECT`. Handler jalan DB query dgn user_idx=0 → load DB tak terduga / edge case stored proc.
- **Fix:** Tambah gate di top `User::PacketParsing`: kecuali `CTPK_CLOSE_SESSION`/`CTPK_CONNECT`/`CTPK_MOVING_SERVER`, wajib `IsConnectProcessComplete()` + `GetUserIndex()!=0`; else drop.

### U-4 — MEDIUM: `lstrlen` OOB read lewati real packet data (RC-2 instance)
- **Lokasi:** `SP2Packet.cpp:548-562` (`>>LPTSTR`), `:619-633` (`>>ioHashString`), `:1119-1133` (`Read(ioHashString)`), `:1149-1179` (`Read(LPTSTR,int)`)
- **Tipe:** Out-of-bounds read / info leak primitive
- **Root cause:** Semua baca string compute `nlen=lstrlen(&m_pBuffer[m_currentPos])` **sebelum** bound check. `CheckRightPacketSize` cek vs MAX_BUFFER (65536) bukan ukuran paket header. Buffer `m_pBuffer[MAX_BUFFER]` reused (tidak selalu di-zero antar paket). Paket pendek dgn string field tanpa null dalam real payload → `lstrlen` scan lewati real payload ke null berikutnya di buffer (data stale dari paket sebelumnya di koneksi itu). Handler proses string lebih panjang dari payload legit.
- **PoC:** Kirim TCP paket dgn header size=50, string field offset N tanpa null dalam 50 byte. `lstrlen` baca lewati byte 50 ke null di buffer 65536 (data stale).
- **Fix:** Lihat P-3 file 01. Bound scan ke `GetBufferSize()-m_currentPos` dgn `memchr`.

### U-5 — MEDIUM: Unbounded loop `iMaxBestFriend` di `UserCopyNode::ApplySyncCreate`
- **Lokasi:** `UserCopyNode.cpp:71-78`; juga `ApplySyncBestFriend` (`:113-124`)
- **Tipe:** Unbounded loop → memory exhaustion / DoS
- **Root cause:**
  ```cpp
  int iMaxBestFriend; rkPacket >> iMaxBestFriend;
  for(int i=0;i<iMaxBestFriend;i++){ DWORD dw; rkPacket >> dw; m_vBestFriend.push_back(dw); }
  ```
  Tidak ada `MAX_GUARD`. Bandingkan `User::ApplyMoveData:1485` yang benar `MAX_GUARD(iSize, 100)`. Peer server (atau tampered sync packet `SSTPK_USER_SYNC_CREATE`) kirim `iMaxBestFriend=0x7FFFFFFF` → loop miliaran `push_back` → OOM/crash.
- **Fix:** `MAX_GUARD(iMaxBestFriend, MAX_BEST_FRIEND)` (mis. 100) sebelum loop; break jika read gagal.

### U-6 — SUSPECTED MEDIUM: Unbounded `iCount` di `User::ApplyAwakeMoveData`
- **Lokasi:** `User.cpp:1695-1703`
- **Tipe:** Unbounded loop / map-insertion DoS
- **Root cause:** `int iCount; PACKET_GUARD_VOID_READ(rkPacket, iCount);` lalu `for(i=0;i<iCount;i++) m_CharAwakeDataMap.insert(...)` tanpa `MAX_GUARD`. Server-move data path. Move packet malicious → unbounded map growth / OOM.
- **Fix:** `MAX_GUARD(iCount, MAX_AWAKE_ENTRY)`.

### U-7 — SUSPECTED MEDIUM: Unbounded `new ioCharacter` di `User::ApplySoldierMoveData`
- **Lokasi:** `User.cpp:1710-1746`
- **Tipe:** Unbounded heap allocation DoS
- **Root cause:** `int iSize; PACKET_GUARD_VOID_READ(rkPacket, iSize);` lalu loop `iSize` kali panggil `AddCharDataToPointer()` (`new ioCharacter` + `m_CharList.push_back`). Tidak ada `MAX_GUARD`. Move packet malicious → unbounded `new` → OOM.
- **Fix:** Clamp `iSize` ke `m_iCurMaxCharSlot` (atau `m_iLimiteMaxCharSlot`); reject overflow.

### U-8 — LOW (latent): Dead-code stack overflow di `OnAbstract`
- **Lokasi:** `User.cpp:12554-12555`
- **Root cause:** Dalam `#if 0`: `char sztemp[32768]={0,}; packet >> sztemp;` — pola vulnerable sama persis. Tidak compile hari ini (live code di `:12567` cuma baca 2 int), tapi jika `#if 0` di-uncomment → 32KB-stack-vs-up-to-64KB overflow → RCE via `CTPK_ABSTRACT`.
- **Fix:** Hapus dead block atau konversi ke `ioHashString`/bounded `Read`.

### U-9 — LOW: `SetClientAddr` truncate IP copy crash
- **Lokasi:** `User.cpp:7095-7102`
- **Root cause:** `char back_iip[16]=""; strcpy_s(back_iip, m_public_ip);`. Jika `m_public_ip` >15 char (IPv6 / malformed via `SetClientAddressForRelay`), `strcpy_s` invalid-parameter handler → process abort. `m_public_ip` normal <16 (dari `inet_ntoa`), tapi `SetClientAddressForRelay(char*)` public setter.
- **Fix:** Buffer 46 byte (`INET6_ADDRSTRLEN`); `StringCbCopy` dgn explicit length check.

### U-10 — LOW: `ioHashString` heap-amplification DoS
- **Lokasi:** Semua `>> ioHashString` reads (chat/memo/friend/present)
- **Root cause:** Tiap field `ioHashString` bisa grow ke ~`MAX_BUFFER` (65535) via `ReAllocCapacity` karena satu-satunya bound `CheckRightPacketSize` vs MAX_BUFFER. Handler baca multiple string attacker-controlled (chat, memo, friend refresh) → single client force banyak ~64KB alloc. Banyak koneksi → amplification DoS.
- **Fix:** Enforce per-field length cap (chat ≤256, ID ≤`ID_NUM`); baca ke bounded buffer atau validate `Length()` setelah read.

### U-11 — SUSPECTED: UAF in-flight packet setelah `OnClose`
- **Lokasi:** `User.cpp:9174-9189`, `UserNodeManager.cpp:222-242`
- **Root cause:** `OnClose` → `g_UserNodeManager.RemoveNode(this)` (return ke pool) → `OnSessionDestroy` → `InitData`. Setelah `RemoveNode`, `User*` mungkin masih direferensikan paket in-flight di `g_RecvQueue` atau struktur relay/battle-room. `ioPacketQueue::ParseSession` cuma cek `pSessionNode->IsActive()` sebelum dispatch; jika paket sudah queued sebelum `OnDestroy` clear state, dispatch ke node half-destroyed. SEH `__except` tangkap AV tapi state corruption (`m_pMyRoom` di-null saat room packet diproses) mungkin.
- **Fix:** Ref-count/deferred-destroy model, bukan immediate `InitData` saat disconnect.

---

## Memory Lifecycle Notes
- `m_CharList` (`new ioCharacter` per `AddCharDataToPointer`) di-free di `User::InitCharList` (`:473-485`) via `SAFEDELETE` per elemen, dari `InitData`. `OnSessionDestroy` → `InitData` cleanup. Tidak leak obvious.
- `m_pEncLoginKey` (`new ioHashString` di `OnConnect:9361`) `SAFEDELETE` sebelum re-assign (`:9360`) dan di `~User`. OK.
- `m_pNS`/`m_pNProtectAuth` free di `~User`. OK.

---

## Prioritas Fix Lokal
1. **U-1 (P0):** `MainProcess.cpp:1401` → bounded `Read`. Pre-auth RCE.
2. **U-2 (P1):** Aktifkan `CheckNS` di dispatch TCP user.
3. **U-3 (P1):** Auth gate pre-login.
4. **U-5/U-6/U-7 (P2):** `MAX_GUARD` semua count peer-driven.
5. **U-4 (P1):** Bounded string scan (lintas RC-2).
