# 04 — Node Managers (Memory Pool)

**Area:** Manager node (user/room/battle-room/ladder/channel/shuffle/server) + memory pool infra.
**File dibaca:** `NodeInfo/UserNodeManager.cpp/.h`, `RoomNodeManager.cpp/.h`, `ServerNodeManager.cpp/.h`, `BattleRoomManager.cpp/.h`, `LadderTeamManager.cpp/.h`, `ChannelNodeManager.cpp/.h`, `MemoNodeManager.cpp/.h`, `Room.cpp/.h`, `BattleRoomNode.cpp/.h`, `ServerNode.cpp/.h`, `CopyNodeParent.cpp/.h`, `include/MemPooler.h`

---

## Arsitektur (penting)
- IOCP `WorkerThread` (×16) **hanya** enqueue received packet ke MPSC queue (`g_RecvQueue.InsertQueue`) + enqueue `CTPK_CLOSE_SESSION` saat socket error. **Semua logic node-manager jalan di single `LogicThread`** (`g_processor->Process` → `ioMainProcess::Process` → `g_RecvQueue.PacketParsing()`), termasuk session, DB result (`PK_QUEUE_QUERY`), scheduler tick, monitoring. Jadi manager **single-threaded** praktis → race cross-thread minim, **tapi** logical UAF/stale-pointer via pool reuse dalam thread tetap relevan.
- Pool infra: `MemPooler<>` (`include/MemPooler.h`). Pre-allocated `new` object di-push saat init; `Remove()/Pop()` distribusi; `Push()` return. Memory pool tidak pernah benar-benar di-free per-object sampai `DestroyPool()` (`delete[]` segment `BYTE[]` dari `Create()` growth).

---

## Mgr-1 — HIGH: DoS via pre-login connection pool exhaustion (zombie)
- **Lokasi:** `UserNodeManager.cpp:145-154,150`; `User.cpp:9205-9214`; `UserNodeManager.cpp:1496,1528`
- **Tipe:** pool exhaustion / DoS
- **Root cause:** User `MemPooler` max 5000 (`m_MemNode.CreatePool(0, 5000, FALSE)` line 150). `m_iMaxConnection` (login cap, default 1500 dari `user_pool`) hanya govern login check di `User::OnConnect` (`:9205`). TCP accept + `CreateNewNode` + `AddUserNode` terjadi **sebelum** login check → sampai 5000 socket admit ke `m_vUserNode` regardless login. Ghost-cleanup loop (`UserNode_GhostCheck`, `:1496` & `:1528`) eksplisit skip node `GetSyncTime()==0` → **pre-login/silent connection tidak pernah di-reap**. Tidak ada per-IP concurrent limit (hanya whitelist/blacklist `User.cpp:9238/9250` per-IP allow/deny, bukan count cap).
- **PoC:** Attacker buka ~5000 TCP ke game port, kirim nothing (atau handshake invalid yang tidak set `m_sync_time`). Tiap slot consume `User` pool di `m_vUserNode`. Saat `GetNodeSize()` ≥ `m_iMaxConnection`, setiap login legit terima `CONNECT_GAMESERVER_FULL` (`User.cpp:9209`). Saat pool 5000, `CreateNewNode` NULL → `OnAccept` hard-close socket (`AcceptorUserNode.cpp:64-72`). Zombie persist sampai socket fisik drop (ghost check skip `SyncTime==0`).
- **Fix:** (1) Reject/limit accept by per-IP count; (2) accept-timeout force-close pre-login + return node; (3) ghost check tidak skip `SyncTime==0` (atau set connect-deadline saat accept); (4) turunkan pool `maximumBlock` ke `m_iMaxConnection` atau gate `CreateNewNode` by `GetNodeSize() < m_iMaxConnection`.

### Mgr-2 — HIGH: Stale `User*` di `m_UserSyncDataVec` → cross-user corruption / UAF
- **Lokasi:** `UserNodeManager.cpp:1411-1465` (`UpdateUserSync`, `UserNode_DataSync`); disconnect `User.cpp:9174-9189`, `UserNodeManager.cpp:222-242`
- **Tipe:** use-after-free / stale pointer (logical, single-thread)
- **Root cause:** `UpdateUserSync(pUser)` simpan raw `User*` (`m_pUser`) ke `m_UserSyncDataVec`, **tidak pernah** remove saat disconnect. Saat disconnect, `User::OnClose` → `g_UserNodeManager.RemoveNode(this)` → `m_MemNode.Push(pUser)` (return ke free pool). `UserNode_DataSync` (50ms tick) iterasi vector, panggil `rkUserSync.m_pUser->IsConnectState()` / `->SyncUserUpdate()`. Antara `RemoveNode` & DataSync berikutnya, accept baru mungkin `CreateNewNode` → `m_MemNode.Remove()` hand **same** `User` block, `OnCreate` flip ke connected, stale entry ref user **beda** → `SyncUserUpdate()` ke user salah (cross-user state write) atau read stale field.
- **PoC:** Attacker A trigger `UpdateUserSync`, disconnect langsung. Attacker B connect di window ~50ms, reuse slot A. DataSync tick run `SyncUserUpdate()` di B pakai queued sync intent A.
- **Fix:** Saat disconnect (`OnClose`/`RemoveNode`), remove entry user dari `m_UserSyncDataVec` (atau invalidate `m_pUser=NULL` — `DataSync` sudah handle di `:1446`). Lebih baik: key sync list by `dwUserIndex`, validate via `GetUserNode()` sebelum use.

### Mgr-3 — MEDIUM: Pool returned ke free list SEBELUM `OnSessionDestroy` selesai (latent UAF)
- **Lokasi:** `User.cpp:9174-9189` (`User::OnClose`), `UserNodeManager.cpp:222-242` (`RemoveNode`)
- **Root cause:** `OnClose` call `OnDestroy()`, lalu `g_UserNodeManager.RemoveNode(this)` (erase container + `m_MemNode.Push(this)` return ke pool), **baru** `OnSessionDestroy()` run di `this`. Object di free pool saat `OnSessionDestroy` eksekusi `LeaveBattleRoom`/`LeaveLadderTeam`/`LeaveShuffleRoom`/`LeaveChannel`/`SaveData`/`SyncUserLogout`. Single-thread + pool tidak overwrite memory → tidak crash *hari ini*, tapi path future di `OnSessionDestroy` yang trigger `CreateNewNode` (atau pool `Remove()`) → hand `User` ini ke koneksi baru saat `OnSessionDestroy` masih mid-execution → true UAF.
- **PoC:** Tidak triggerable saat ini, tapi rapuh.
- **Fix:** Reorder `OnClose`: `OnDestroy()` → `OnSessionDestroy()` → `RemoveNode(this)`. `RemoveNode` harus operasi terakhir yang touch `this`.

### Mgr-4 — MEDIUM: `MemPooler::Push` no duplicate guard → double-use
- **Lokasi:** `MemPooler.h:55-67` (`Push`/`Push_Front`)
- **Root cause:** `Push(block)` blind `push_back` pointer; tidak cek apakah `block` sudah di deque. Jika manager panggil `Remove*` di node sama dua kali (atau return ke pool tanpa remove dari container), pointer sama di-hand ke **dua** consumer → keduanya operasi memory sama → corruption / UAF. Manager `Remove*` guard dgn find-then-erase (aman saat ini), tapi pool tidak defense-in-depth. `Push` tidak update `m_numofBlock` accounting (hanya `Create`).
- **Fix:** Debug-mode duplicate check di `Push` (assert tidak di deque, atau pakai set); atau generation counter di node.

### Mgr-5 — MEDIUM: `MemPooler::Get()` UB saat `maximumBlock == 0`
- **Lokasi:** `MemPooler.h:112-136` (`Get`)
- **Root cause:** Branch else (pool empty, grow): `int allocCount = m_maximumBlock - m_numofBlock;`. Jika `m_maximumBlock==0` → underflow ke large negative; `Create(allocCount)` no-op; deque empty; `block = m_memPooler.front();` di **empty** `std::deque` → UB (crash). Semua pool saat ini `maximumBlock>0`, tapi `MemPooler` default-constructed (`maximumBlock=0`, mis. `MemPooler<RelayGroup> m_vRelayGroupPool;`) lalu exhausted → crash.
- **Fix:** Guard `Get()`: re-check `!m_memPooler.empty()` setelah growth; reject `maximumBlock==0`; return NULL.

### Mgr-6 — MEDIUM: `MemPooler::Destroy` tidak invoke destructor → leak node-internal state
- **Lokasi:** `MemPooler.h:97-110` (`Destroy`); init `UserNodeManager.cpp:153`, `RoomNodeManager.cpp:87`, `BattleRoomManager.cpp:52`, `LadderTeamManager.cpp:58`, `ChannelNodeManager.cpp:44`, `ShuffleRoomManager.cpp:52`, `ServerNodeManager.cpp:50`, `ServerNode.cpp:75`
- **Root cause:** Dua style alokasi:
  1. Pre-pushed `new User(...)`/`new Room(...)` (heap) — `Destroy()` hanya `delete[]` `m_memBlocks` (placement-new `BYTE[]` dari `Create()`), jadi **semua pre-pushed `new` object tidak pernah di-free**.
  2. `Create()`-grown block pakai placement-new ke `BYTE[]`; `Destroy()` `delete[] memBlock` **tanpa call `~Type()`** → `User`/`Room`/`BattleRoomNode` internal member (`std::string`, `std::vector`, `ioHashString`, mode pointer) leak allocation.
  Lifetime manual via `OnCreate/OnDestroy` → mostly shutdown leak, tapi `OnDestroy` harus perfect release setiap member atau live leak akumulasi.
- **Fix:** Track pre-pushed heap object terpisah & `delete` di `Destroy`; call `~Type()` (placement destructor) sebelum `delete[]` growth block; atau alokasi semua via `Create()` (single `BYTE[]`).

### Mgr-7 — MEDIUM: Inconsistent thread-safety setting pool (latent race surface)
- **Lokasi:** `UserNodeManager.cpp:150` (FALSE), `RoomNodeManager.cpp:84` (FALSE), `BattleRoomManager.cpp:49` (FALSE), `LadderTeamManager.cpp:55` (FALSE), `ChannelNodeManager.cpp:41` (FALSE), `ShuffleRoomManager.cpp:49` (FALSE), `ServerNodeManager.cpp:47` (FALSE) — vs — `ServerNode.cpp:73,91,102,115,128,140` (copy-node pool = **TRUE**)
- **Root cause:** Pool "original" `threadSafe=FALSE` (`MemPooler::Lock/Unlock` no-op), copy-node pool `threadSafe=TRUE`. Logic single-thread → aman, tapi copy pool locked sementara original tidak → indikasi uncertainty developer. Path future (DB-agent callback thread, atau `ProcessFlush`/`DrawModule` ke thread lain) yang touch original pool → `FALSE` silently disable lock → race di `std::deque` & `std::vector`/`std::map` manager.
- **Fix:** Semua pool `threadSafe=TRUE` defense-in-depth, atau formally document & enforce single-thread invariant (assert logic-thread ID).

### Mgr-8 — MEDIUM: `RemoveUserCopyNode`/`RemoveBattleRoomCopyNode` dapat drop room ke pool saat iterasi via `LeaveUser`
- **Lokasi:** `BattleRoomManager.cpp:180-193` (`RemoveUserCopyNode`) → `BattleRoomNode.cpp:664-675` (`LeaveUser` → `RemoveBattleRoom(this)`); `LadderTeamManager` analog (`LadderTeamNode.cpp:343-351` → `RemoveLadderTeam`)
- **Root cause:** `RemoveUserCopyNode(dwUserIndex)` iterasi `m_vBattleRoomNode` panggil `pCursor->LeaveUser(...)`. Jika `LeaveUser` hapus user terakhir → `g_BattleRoomManager.RemoveBattleRoom(this)` erase `pCursor` dari `m_vBattleRoomNode` & return ke pool — saat iterator `iter` masih point ke sana. Function `return` langsung setelah hit pertama → iterator invalidated tidak di-increment (aman saat ini). Tapi refactor yang hapus early `return`, atau match kedua di call sama → increment dangling → UAF/crash.
- **Fix:** Collect room-to-leave ke local list dulu; atau index-based iterasi dgn `iter=begin()` restart; atau `LeaveUser` tidak self-remove (defer ke manager tick).

### Mgr-9 — MEDIUM (SUSPECTED): `RoomProcess` two-pass plaza capture `iEnd` lalu mungkin grow
- **Lokasi:** `RoomNodeManager.cpp:1320-1352`
- **Root cause:** Loop plaza pertama (`:1320-1327`) capture `iEnd = m_vPlazaNode.end()` lalu iterasi `(*iter)->RoomProcess()`. `Room::RoomProcess` → `m_pMode->ProcessTime()`/`ProcessReserveUser()` (`Room.cpp:1936-1965`). Jika salah satu (mis. `ProcessReserveUser`, mode `ProcessTime`, relay callback) create plaza baru (`CreateNewPlazaRoom` → `m_vPlazaNode.push_back`/`insert`) → `iEnd` & `iter` invalidated oleh reallocation → UAF/crash. Pass kedua (`:1331-1352`) re-capture `iEnd` (aman) tapi tetap call room. Tidak diketahui create plaza dari dalam `RoomProcess` saat ini → SUSPECTED.
- **Fix:** Index-based iterasi (`for(size_t i=0; i<m_vPlazaNode.size(); ++i)`) supaya growth tolerate; atau snapshot room list ke local vector sebelum process.

### Mgr-10 — LOW: `UserNodeManager::RemoveNode` silently leak pool slot jika tidak ketemu
- **Lokasi:** `UserNodeManager.cpp:222-242`
- **Root cause:** `RemoveNode` search `m_uUserNode` by `GetUserIndex()`; jika tidak, linear scan `m_vUserNode` by `GetEntity()`; jika tidak di keduanya → tidak lakukan apa-apa — node tidak return ke pool (slot leak) & caller (`OnClose`) tetap lanjut `OnSessionDestroy`. Guard `IsDisconnectState` di `OnClose` cegah double-cleanup → leak aktif tidak teramati, tapi flow future yang `RemoveNode` di user already-moved/never-added → slot hilang.
- **Fix:** `RemoveNode` selalu return node ke pool (atau assert found); atau `OnClose` return ke pool unconditional & container self-cleaning.

### Mgr-11 — LOW (SUSPECTED): `LadderTeamManager::UpdateProcess` map iterasi saat `Process()`/match-resolution mungkin erase
- **Lokasi:** `LadderTeamManager.cpp:1569-1590`, `LadderTeamNode.cpp:1480-1507`
- **Root cause:** `UpdateProcess` iterasi `m_mLadderTeamNode` (`std::map`) `it++` panggil `pNode->Process()`. `Process` hanya flip state (no erase), `IsReserveTimeOver` collect ke `vDestroyNode` remove **setelah** loop — aman. Tapi `RemoveSearchingList` tidak touch `m_mLadderTeamNode`. Pola rapuh: future `Process()` yang `RemoveLadderTeam(this)` (`LadderTeamNode.cpp:350`) di loop → invalidate `it` → UB.
- **Fix:** Snapshot key atau `it = erase(it)` pattern; defer removal post-loop.

### Mgr-12 — LOW: `MemoNodeManager::SendMemo` deref `pUser` tanpa NULL check
- **Lokasi:** `MemoNodeManager.cpp:54-65`
- **Root cause:** `SendMemo(UserParent *pUser, ...)` langsung `pUser->GetPublicID()` di `:65` tanpa null guard (hanya `CRASH_GUARD()` SEH `:56` yang swallow AV). Caller umumnya pass live user, tapi relay/async path bisa pass user yang baru disconnect.
- **Fix:** `if(!pUser) return;` di top.

### Mgr-13 — LOW: `static` scratch container di manager (reentrancy hazard)
- **Lokasi:** `RoomNodeManager.cpp:1300,194`, `BattleRoomManager.cpp:199`, `ChannelNodeManager.cpp:155`, `MemoNodeManager.cpp:57`, `LadderTeamManager.cpp:1556`, dll.
- **Root cause:** Banyak manager pakai `static` local `vector` scratch. Single-thread + non-reentrant → aman, tapi `static` → future reentrancy (mis. `SendMessage` yang sync parse reply, atau DB callback re-enter manager) corrupt shared scratch.
- **Fix:** Stack-local (`vScratch vTmp;`); reserve capacity cegah reallocate.

### Mgr-14 — LOW: `ServerNodeManager::RemoveNode(ServerNode*)` tidak call `OnDestroy` (asymmetric)
- **Lokasi:** `ServerNodeManager.cpp:278-290` vs `:292-306`
- **Root cause:** `RemoveNode(ServerNode*)` erase `m_vServerNode` + `Push` ke pool tapi **tidak** `pServerNode->OnDestroy()`, sedangkan `RemoveNode(int iServerIndex)` (`:301`) call `OnDestroy()`. Pointer overload dipanggil dari `ServerNode::SessionClose`-driven cleanup → skip `OnDestroy` → copy-node map remote (`m_UserCopyNode`, `m_RoomCopyNode`) + registrasi global (`g_UserNodeManager::m_uUserCopyNode`) tidak dibersihkan di path itu → stale copy node tetap terdaftar global setelah `ServerNode` di-recycle. (Full cleanup sebenarnya via `ServerNode::OnDestroy` → `Return*MemoryPool`.)
- **Fix:** Call `OnDestroy()` di pointer overload juga, atau funnel keduanya ke 1 cleanup.

### Mgr-15 — LOW: Copy-node lifecycle dual-registration
- **Lokasi:** `ServerNode.cpp:84-186`, `UserNodeManager.cpp:310-374`
- **Root cause:** `UserCopyNode` terdaftar di 2 tempat: owning `ServerNode::m_UserCopyNode` (by index) + global `UserNodeManager::m_uUserCopyNode` (+ ID tables). Removal butuh `ServerNode::RemoveUserNode` (call `g_UserNodeManager.RemoveCopyUser`) **dan** ID-table cleanup. Dual-registration = source classic dangling global entry jika path removal lupa satu sisi (lihat Mgr-14).
- **Fix:** Single owner: global manager hold weak ref ke owning `ServerNode`, delegate cleanup ke 1 path.

---

## Bukan Temuan (untuk kelengkapan)
- Tidak ada active data race di node manager dari IOCP worker (worker hanya enqueue + `CTPK_CLOSE_SESSION` marker; semua mutasi manager di single `LogicThread`). `threadSafe=FALSE` pool aman saat ini (Mgr-7 latent).
- Pool-exhaustion NULL deref plaza/room creation di-handle — caller `CreateNewPlazaRoom`/`CreateNewHeadquartersRoom`/`CreateNewRoom` cek NULL return (`User.cpp:3005,11079`; `ServerNode.cpp:2649`) & fallback lobby. DoS real = Mgr-1 (user pool zombie), bukan NULL crash.
- `BattleRoomManager::UpdateProcess`/`ShuffleRoomManager::UpdateProcess` deferred-destroy pattern (collect `vDestroyNode`, remove post-loop) benar; tidak self-remove saat `Process()`.
- `DrawModule`/monitoring stats gather di LogicThread (dari `MainServerNode` handler), bukan thread terpisah → tidak cross-thread read race di `GetNodeSize()`/`RemainderNode()`.

---

## Prioritas Fix Lokal
1. **Mgr-1 (P2):** Pre-login zombie DoS — per-IP cap, accept-timeout, ghost check tidak skip.
2. **Mgr-2 (P2):** `m_UserSyncDataVec` stale — remove saat disconnect / key by index.
3. **Mgr-3 (P3):** Reorder `OnClose`.
4. **Mgr-4/Mgr-5/Mgr-6/Mgr-7 (P3):** `MemPooler` hardening (duplicate check, empty guard, destructor call, threadSafe).
