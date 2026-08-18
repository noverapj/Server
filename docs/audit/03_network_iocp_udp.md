# 03 — Network / IOCP / UDP / Relay

**Area:** Lapisan network paling exposed ke client.
**File dibaca:** `Network/*` (GameServer, GameSvrUDPModule, GameSvrUDPNode, ioBroadCastRelayModule, iocpHandler, ioPacketQueue, ioServerSecurity, SP2Packet), `iocpSocketDLL/*` (SocketModules/, ThreadModules/, IOCP/), `MainProcess.cpp`

---

## N-0 — HIGH (enabler): Crypto keyless & forgeable
- **Lokasi:** `iocpSocketDLL/Encrypt/cryption.cpp:5-49`, `SocketModules/ioUDPSecurity.cpp:32-66`, `Network/ioServerSecurity.cpp:66-100`, `Util/NetworkSecurity.cpp:16-51`
- **Tipe:** Broken cryptographic design
- **Root cause:**
  - `Encrypt`/`Decrypt` = `XOR 0xFF` + rotate-left-3 (no key).
  - `MakeDigest` = plain MD5-XOR-fold ke DWORD (no HMAC, no salt, no key).
  - Checksum `ioUDPSecurity`/`ioServerSecurity` = `MakeDigest(buffer, GetBufferSize())`.
  - Base `NetworkSecurity` stub semua check `return true`; `UDPNode::CheckNS` return TRUE saat `ns==NULL` (`UDPNode.cpp:264`); `CConnectNode::CheckNS` return true saat `m_pNS` null (`ConnectNode.cpp:311`).
  - Algo embed di binary yang di-ship → attacker reverse → forge checksum valid untuk paket sembarang.
- **PoC:** Reverse `Encrypt`/`MakeDigest` dari binary, compute checksum+state valid untuk crafted packet. Semua "checksum verified" attack path (N-1..N-4) jadi reachable.
- **Fix:** Ganti dgn authenticated-encryption per-session (AES-GCM dgn session key dari login). `CheckState` per-connection; reject `MAGIC_TOKEN_FSM` setelah paket pertama. Null `NetworkSecurity` = deny-by-default.

### N-1 — CRITICAL: Stack buffer overflow via UDP string read (sama U-1)
- **Lokasi:** `MainProcess.cpp:1401-1402`, enabled `Packet.cpp:291`
- Lihat detail U-1 di file 02. Pre-auth single-UDP RCE.

### N-2 — CRITICAL: UDP relay SSRF / reflection amplification ke IP:port arbitrary
- **Lokasi:** `MainProcess.cpp:1518-1555` (`OnRelayUDPParse`); mirror `ioBroadCastRelayModule.cpp:136-210` (`SendPacket`) & `:1397-1429` (`Broadcast` ANTIHACK)
- **Tipe:** SSRF / UDP reflection-amplification DoS
- **Root cause:** Untuk paket `PacketID ∈ [CUPK_CONNECT, 0x5000)`, server baca `dwIP` & `dwPort` dari body, panggil `g_UDPNode.SendMessage(szIP, dwPort, kPacket)`. Validasi tunggal: self-check `dwIP==g_App.GetDwordIP() && dwPort==g_App.GetCSPort()` — hanya CS port sendiri yang di-block. Host lain di internet + port lain di server sendiri (SS, monitoring, internal) reachable. Tidak ada allowlist peer; tidak ada per-source rate limit di path UDP (lihat N-8).
- **PoC:** Forge UDP relay packet `PacketID` relay-range, body `{dwIP=victim_IP, dwPort=victim_port, payload}`. Server kirim UDP datagram ke `victim_IP:victim_port` berisi payload attacker-controlled. Botnet forged source → amplifikasi DoS ke pihak ketiga, atau probe/abuse internal service (SSRF ke monitoring port 127.0.0.1, DB internal, cloud metadata).
- **Fix:** Allowlist peer (relay-server/game-server) IP:port legit; tolak private/loopback/multicast kecuali intend. Apply allowlist di `OnRelayUDPParse` & `ioBroadCastRelayModule::SendPacket/Broadcast`.

### N-3 — CRITICAL: `CUPK_TEST` reflection primitive unconditional (bypass self-check)
- **Lokasi:** `ioBroadCastRelayModule.cpp:149-164` (non-ANTIHACK `SendPacket`)
- **Root cause:** Sebelum handling relay umum, special case:
  ```cpp
  if( kPacket.GetPacketID() == CUPK_TEST ) {   // 0x4008, dalam [CUPK_CONNECT,0x5000)
      SP2Packet kRPacket = kPacket;
      DWORD dwIP=0, dwPort=0;
      kPacket  >> dwIP >> dwPort;
      g_UDPNode.SendMessageByDWORDIP( dwIP, dwPort, kRPacket );
      return TRUE;
  }
  ```
  **Tidak** ada self-IP check (self-check di line 202 setelahnya). `CUPK_TEST` echo full packet (termasuk payload) ke `dwIP:dwPort` attacker-chosen. Reachable oleh client UDP yang forge checksum keyless.
- **PoC:** Kirim UDP `PacketID=0x4008`, body `{dwIP, dwPort, garbage}`. Server kirim `garbage` (+ relay header) ke `dwIP:dwPort`. Amplifikasi bandwidth / launder origin. Bisa self-flood (self-check skip).
- **Fix:** Hapus `CUPK_TEST` relay di production; atau restrict ke trusted internal IP + token auth. Apply allowlist sama N-2.

### N-4 — HIGH: Cross-user / cross-room packet injection via forged `dwUserIndex`
- **Lokasi:** `MainProcess.cpp:1527-1540` (`OnRelayUDPParse`); `ioBroadCastRelayModule.cpp:178-200` (`SendPacket` non-ANTIHACK)
- **Tipe:** Authentication bypass / impersonation / state tampering
- **Root cause:** Saat relay packet `dwIP==0 && dwPort==0`, handler baca `dwUserIndex` dari body (fully attacker-controlled), dispatch sisa payload ke session victim:
  ```cpp
  DWORD dwUserIndex; rkPacket >> dwUserIndex;
  User *pUser = g_UserNodeManager.GetUserNode(dwUserIndex);
  if( pUser && pUser->IsConnectState() ) pUser->PacketParsing(rkPacket);
  ```
  "Sender" tidak dikorelasikan dgn `dwUserIndex` — peer UDP mana saja target user connected mana saja. Non-ANTIHACK `SendPacket` tambah broadcast ke seluruh room via `SendRelayPacket(dwUserIndex, kRelayPacket)`. Gate tunggal: UDP checksum keyless (forgeable).
- **PoC:** Forge UDP packet `PacketID` relay-range (bukan CONNECT/SYNCTIME/RESERVE/CHECK_KING_PING), body `{dwIP=0, dwPort=0, dwUserIndex=victim, <gameplay payload>}`. Server parse `<gameplay payload>` seolah dari victim → inject move/spawn/skill/damage atas nama victim, atau broadcast ke room victim.
- **Fix:** Bind relay routing ke source terauth: hanya terima `dwUserIndex` yang match user terkait source IP:port (set saat `CUPK_CONNECT`). Tolak jika `dwUserIndex` tidak match sender.

### N-5 — HIGH: Weak/bypassable per-packet auth (keyless + bypassable FSM)
- **Lokasi:** `ioUDPSecurity.cpp:32-107`, `ioServerSecurity.cpp:86-158`, `NetworkSecurity.cpp`
- Lihat N-0. `ioUDPSecurity::CheckState` hanya block re-send exact FSM state dari same IP:port; paket pertama (state `MAGIC_TOKEN_FSM==-1`) selalu accept; ganti port evades replay. Single `ioUDPSecurity` instance shared semua client di socket UDP → cross-interfere, tidak ada replay protection per-client. Null `NetworkSecurity` = allow.
- **Fix:** Real per-session authenticated-encryption; per-connection replay; deny-by-default null.

### N-6 — MEDIUM: `PacketID`/`Size` header trusted as packet delimiter
- **Lokasi:** `ConnectNode.cpp:220-242` (`DispatchReceive`), `UDPNode.cpp:208-243`, header `Packet.h:6-13`
- **Tipe:** Trusting attacker length field / packet forgery
- **Root cause:** `packet.GetBufferSize()` = `*m_packet_header.m_Size` (attacker-controlled). Receive loop pakai size attacker sebagai authoritative packet length. Client set size field lebih kecil dari body → sisa datagram re-parse sebagai "packet" baru dgn header attacker-chosen. `SetBufferCopy` clamp `min(MAX_BUFFER,size)` cegah heap overflow, tapi framing desync = forgeability primitive yang amplify N-1..N-4.
- **PoC:** Kirim 1 UDP datagram dgn size field=16 (header only). "Paket" pertama 16 byte; byte sisanya re-parse sebagai paket kedua dgn ID/checksum/size attacker. Smuggle packet lewat naive length filter.
- **Fix:** Treat `m_Size` advisory; advance cursor tidak lebih dari actual bytes; validate `m_Size` vs expected per-PacketID size table. Strip/ignore trailing bytes invalid.

### N-7 — MEDIUM: No TCP connection limit / accept-flood DoS
- **Lokasi:** `ServerSocket.cpp:110-119` (`Run` accept loop), `AcceptorUserNode.cpp:28-35,49-72`
- **Root cause:** `ServerSocket::Run` loop `accept()` unconditional, hand socket ke `AcceptorUserNode::ReceivePacket` (enqueue accept-packet). Backpressure hanya indirect: jika accept `PacketQueue` pool (`PacketPool::m_poolerAccept`, `CreatePool(100,1000,TRUE)` `PacketPool.cpp:24`) atau user-node pool (`UserNodeManager::InitMemoryPool`) habis, socket di-close. Tidak ada cap concurrent accept, tidak ada per-IP limit, tidak ada accept-rate throttle. Attacker buka ribuan TCP (slow/half-open) → habis pool 1000 + user-node pool → deny player baru. `AcceptorUserNode::OnAccept` cuma cek `CreateNewNode` NULL; tidak ada global max-connected gate sebelum accept.
- **Fix:** Hard cap total active/incomplete connections + per-source-IP connection-rate limit di accept (`WSAAccept` condition function); `SO_LINGER`/timeout untuk handshake incomplete.

### N-8 — HIGH: No DoS rate limiting di path UDP
- **Lokasi:** `NetworkSecurity.cpp:16-19` (`UpdateReceiveCount` stub), `ioUDPSecurity` (tidak override), vs `ioServerSecurity.cpp:37-63` (TCP override) & `User.cpp:1919-1922` (`InitDoSAttack`)
- **Root cause:** `ioUDPSecurity` inherit base `UpdateReceiveCount()` return `true` unconditional. `UDPNode::CheckNS` (`UDPNode.cpp:258-271`) panggil `IsCheckSum`+`CheckState` tapi tidak rate-limit. Tidak ada per-source/global UDP packet-rate cap. Checksum forgeable (N-0) → attacker flood *valid-looking* UDP yang semua lewat `CheckNS` → reach `ReceivePacket` → relay queue + logic thread. Relay buffer pool (`BufferPool`, `ioBroadCastRelayModule.cpp:34`) & MPSC relay queue kapasitas finite; flood sustain → drop traffic legit (`BufferPool::Get` NULL, `ioBroadCastRelayModule.cpp:266-278`).
- **Fix:** Per-source-IP UDP rate limit di `NetworkSecurity` subclass UDP (mirror `ioServerSecurity::UpdateReceiveCount`); enforce di `UDPNode::CheckNS`; global UDP packets/sec ceiling drop-on-overflow; per-IP ban setelah checksum fail berulang.

### N-9 — LOW: Memory leak pool pada unknown relay packet ID
- **Lokasi:** `ioBroadCastRelayModule.cpp:80-82` (non-ANTIHACK `Run`), `:1355-1357` (ANTIHACK `ProcessPacket` `default`)
- **Root cause:** Relay worker `switch(pRelayHeader->m_packetId)` handle `UDP_INSERTDATAPACKET`/`UDP_REMOVEPACKET`/`UDP_SENDPACKET` (+ ANTIHACK timer/score), masing-masing `m_bufferPool.Push(...)`. `default:` hanya log, **tidak** return buffer ke pool. `m_packetId` set oleh producer trusted → normal dormant, tapi corruption/future bug enqueue ID unhandled → leak 1 buffer per packet, `BufferPool` fixed pool → leak permanen sampai restart.
- **Fix:** Selalu recycle `pRelayHeader` ke `m_bufferPool` di `default` (+ assert/log).

### N-10 — LOW: `SendIO::SendMessage` swallow failure / send-count drift
- **Lokasi:** `SendIO.cpp:21-42,44-169,171-212`
- **Root cause:** `CSendIO::SendMessage(CPacket&,BOOL)` panggil underlying, on failure hanya log lalu `return true;` → caller percaya terkirim. Saat `g_SendBufferManager->Pop()` NULL (`SendIO.cpp:94/113/133`), return false & member send-buffer state mungkin inconsistent; `PacketSend` return buffer pada `WSASend` sync error (`:203`) tapi rely IOCP completion (`WorkerThread.cpp:39-43`) untuk `ReturnSendBufferMemoryPool`. Jika completion lost / `WSASend` fail race `WSA_IO_PENDING` not set → buffer leak & `m_sendCount` inflate → block `EnableSend` (`ConnectNode.cpp:101-109`) permanen.
- **Fix:** Propagate failure; ensure setiap `CSendBuffer` return ke pool di semua error path; `m_sendCount` symmetric (increment setelah post sukses, decrement di setiap completion + error).

### N-11 — LOW-MEDIUM (SUSPECTED): Shared `ioUDPSecurity` race antar worker
- **Lokasi:** `UDPNode.cpp:245-271` (1 `ioUDPSecurity*` per socket, semua worker); `ioUDPSecurity.cpp:20-107` (non-atomic fields)
- **Root cause:** Tiap UDP receive socket punya single `ioUDPSecurity` (`m_recvInfos[index]->m_pns`). Multiple IOCP UDP worker thread (`UDPWorkerThread`) concurrently `DispatchReceive` di socket itu → `pns->RcvPeerInfo` (write `m_recent_rcv_ip`/`m_pre_recent_rcv_*` non-atomic) lalu `CheckNS` (read). `m_RcvState` = `FSM` `m_iState` non-atomic (`FSM.h:11`; `ioState` di `ioUDPSecurity.h:6-25` pakai `InterlockedExchange`, tapi `FSM m_RcvState` tidak). Concurrent → FSM/replay state inconsistent, spurious accept/reject.
- **Fix:** Per-call `ioUDPSecurity` (stateless) atau guard dgn critical section / `InterlockedExchange` konsisten; idealnya replay protection per-source map protected.

### N-12 — MEDIUM (SUSPECTED): Integer underflow → oversized memcpy di relay slice
- **Lokasi:** `ioBroadCastRelayModule.cpp:178-200` (non-ANTIHACK `SendPacket`)
- **Root cause:** `dwIP==0` branch compute `kPacket.GetDataSize() - iCutSize` (iCutSize=8) tanpa re-verify `GetDataSize() >= iCutSize`. `GetDataSize()` = `*m_Size - sizeof(PACKETHEADER)`. Jika `m_Size` forged antara 16 dan 23 → `GetDataSize()` (4..7) - 8 underflow unsigned → `0xFFFFFFFC`. `SetDataAddCreateUDP(..., 0xFFFFFFFC)` → `memcpy` ~4GB → crash/corrupt. `m_Size` attacker-controlled, hanya bounded `>=16` (via `IsValidHeader`).
- **PoC:** Forge UDP relay `m_Size=20` (header 16 + 4 byte), body `{dwIP=0, dwPort=0, dwUserIndex=any}`. `dwIP` read OK (4 byte), `dwPort`/`dwUserIndex` default 0 via short-circuit. `GetDataSize()-iCutSize` = `(20-16)-8` = `4-8` → `0xFFFFFFFC` → memcpy ~4GB → crash.
- **Fix:** Validate `GetDataSize() >= iCutSize` sebelum subtract; signed/checked arithmetic; reject `m_Size` tidak match plausible body length untuk `PacketID`.

### N-13 — LOW (SUSPECTED): Queue-0 / uninitialized index starve relay
- **Lokasi:** `ioBroadCastRelayModule.cpp:603-610,638-646` (non-ANTIHACK), `:1148-1155,1181-1189` (ANTIHACK); member `m_iQueueIndex` hanya init di ANTIHACK ctor (`:671`)
- **Root cause:** Tiap relay worker panggil `GetMyqueue()` sekali = `Getqueue(InterlockedIncrement(&m_iQueueIndex))` = `val % m_queues.size()`. ANTIHACK `m_iQueueIndex` start 0 → first `InterlockedIncrement` = 1 → queue `1 % size`; **queue 0 tidak pernah di-assign worker**. `EnqueueByIndex(dwRoomIndex,...)` route `dwRoomIndex % size` (bisa 0) → queue 0 pile up (memory growth + room relay freeze). Non-ANTIHACK `m_iQueueIndex` **tidak** init di ctor (`:20-23` hanya `m_queueId=-1`) → indeterminate UB.
- **Fix:** Init `m_iQueueIndex=-1` di kedua build (first increment = 0), atau assign worker ke queue by creation order; pastikan tiap queue punya 1 drainer.

### N-14 — LOW: `strcpy_s` ke fixed `m_szPublicIP[STR_IP_MAX=64]` tanpa length check
- **Lokasi:** `ioBroadCastRelayModule.cpp:240,463,793,995`; struct `Define.h:1081,1090,1161,1215,1887`
- **Root cause:** `strcpy_s(dst, src)` 2-arg pakai `sizeof(dst)` cap saat dst fixed array → `strcpy_s` fail-fast (abort) on overflow, bukan overwrite. Field `m_szPublicID[ID_NUM_PLUS_ONE]` sama. Input server-controlled (room/user data) → defense-in-depth. Overlong → process abort DoS.
- **Fix:** Validate length sebelum copy; `strcpy_s(dst, sizeof(dst), src)` dgn prior check; reject overlong IP/ID di ingress (login).

### N-15 — LOW: `RecvQueue::InsertQueue` trust attacker `GetBufferSize()` untuk pool bucketing
- **Lokasi:** `MPSCRecvQueue.cpp:72-83`, `RecvQueue.cpp:78-91`, `PacketPool.cpp:31-54`, `PacketQueue.cpp:37-60`
- **Root cause:** `PacketPool::Pop(packet.GetBufferSize())` select bucket dari attacker-controlled size. `PacketQueue::Set` `Realloc` jika `m_maxSize < GetBufferSize()` → small bucket elemen bisa grow large `m_buffer`. `PacketPool::Push` re-bucket by `GetDefaultSize()` (original 64) → 64-bucket simpan elemen 32768-byte alloc. Repeated forged size (small declared, large actual) waste memory + skew pool. Forged large size → Big pool (kecil: `CreatePool(10,1000,TRUE)` `PacketPool.cpp:28`) cepat habis → drop legit.
- **Fix:** Bucket by actual received byte count, bukan forged size; cap `Realloc` growth; enforce per-source UDP rate limit (N-8).

### N-16 — LOW: `__try/__except` relay worker mask exploitable crash + leak
- **Lokasi:** `ioBroadCastRelayModule.cpp:1293-1365` (ANTIHACK `ProcessPacket`), `ioPacketQueue.cpp:45-67,87-108,110-133,158-173`
- **Root cause:** Relay worker wrap dispatch `__try/__except(ExceptCallBack(...))` hanya log. Malformed packet trigger AV (mis. N-12 underflow / null-deref) ditelan, thread jalan, tapi `pRelayHeader` **tidak** recycle di exception path → leak, crash di-mask (no abort, no core) → exploit stealth + repeat.
- **Fix:** Di `__except` path, tetap recycle `pRelayHeader` jika sudah dequeue; rate-limit crash-inducing source; log context (packet ID, source IP) untuk alert. Consider crash-and-restart jika integrity uncertain.

---

## Prioritas Fix Lokal
1. **N-1/N-2/N-3 (P0):** Bounded read di UDP dispatch; allowlist peer relay; hapus `CUPK_TEST`.
2. **N-8 (P2):** UDP rate limit; per-IP ban.
3. **N-4 (P0):** Bind relay routing ke source auth.
4. **N-0/N-5 (P0):** Real crypto per-session.
5. **N-12 (P1):** Bounds check sebelum subtract di relay slice.
6. **N-7 (P2):** TCP connection cap + per-IP limit.
