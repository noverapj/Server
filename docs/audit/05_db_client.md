# 05 — DB Client

**Area:** Database client & query/result serialization.
**File dibaca:** `DataBase/DBClient.cpp/.h`, `LogDBClient.cpp/.h`, `QueryData/*`, `Network/SP2Packet.cpp`, `iocpSocketDLL/SocketModules/Packet.*`, `NodeInfo/UserNodeManager.cpp` (result dispatch)

---

## Arsitektur (penting)
Game server **tidak** build SQL text. Serialize `QueryHeader` (proc number `nQueryId` + typed `cSerialize` field blob + `ValueType` descriptors + return-data echo) via plain TCP ke **DB Agent** process terpisah yang turn into real SQL. Jadi SQL injection klasik di layer game-server bergantung pada cara DB Agent bind value (out-of-repo). Sinyal: legacy `sprintf_s(... '%s' ..., name)` commented-out + `Help::GetSafeTextWriteDB()` (SQL-double `'`) apply ke **beberapa** field teks → strong imply DB Agent **string-concatenate**, bukan parameter-bind.

---

## DB-1 — HIGH: `>> CQueryResultData` bad bounds + bad_alloc DoS
- **Lokasi:** `SP2Packet.cpp:678-691` (`>> CQueryResultData`), `:1236-1249` (`Read(CQueryResultData&)`), root `QueryData/QueryResultData.cpp:185-192` (`SetBuffer`)
- **Tipe:** Missing/incorrect bounds → heap DoS + untrusted-data parsing
- **Root cause:** Size check pakai `arg.GetResultBufferSize()` **sebelum** `arg.SetBuffer()` dipanggil. Fresh `CQueryResultData` punya `nResultBufferSize=0` → `CheckRightPacketSize(sizeof(QueryResultHeader) + 0)` reduce ke "header inside 64KB scratch buffer" — tidak validate actual result size yang nanti di-read. `SetBuffer` lalu:
  ```cpp
  memcpy(&m_queryResultHeader, buffer, sizeof(QueryResultHeader));
  m_pBuffer = new char[m_queryResultHeader.nResultBufferSize];   // signed int -> size_t
  memset(m_pBuffer, 0, m_queryResultHeader.nResultBufferSize);
  memcpy(m_pBuffer, buffer+sizeof(QueryResultHeader), m_queryResultHeader.nResultBufferSize);
  ```
  `nResultBufferSize` = signed `int` dari wire, tidak range-check vs sisa paket atau `MAX_BUFFER`. `CheckRightPacketSize` (`Packet.cpp:118`) cek vs `MAX_BUFFER` (65536) tetap.
- **PoC:** DB Agent compromised (atau attacker reach/MITM link DB-Agent unauth — `DBAgentNode::CheckNS` return true, plain TCP) kirim `DTPK_QUERY` result dgn `QueryResultHeader.nResultBufferSize = -1` (0xFFFFFFFF as size_t). `new char[4GiB]` → `std::bad_alloc`; tidak ada catch di `UserNodeManager::GlobalQueryParse` (`:2270-2273`) / dispatch → thread/main terminate → server crash DoS. Alternatif `nResultBufferSize=60000` dgn real payload hanya `sizeof(header)`: alloc OK, `memcpy` read 60000 byte dari region zeroed scratch lewat real payload; setiap `GetValue()` parse zero/garbage sebagai DB rows legit.
- **Fix:** Validate `nResultBufferSize` vs actual remaining packet **sebelum** alloc: `m_currentPos + sizeof(QueryResultHeader) + nResultBufferSize <= GetBufferSize()`; reject negative/oversized. Reorder `>>` read header → check → copy.

### DB-2 — HIGH (SUSPECTED): Crafted DB result parsed as trusted game data → logic corruption
- **Lokasi:** `UserNodeManager.cpp:2270-2359+` (dispatch), `3341-3430` (`OnResultSelectUserData`), `3897-3987` (`OnResultLoginSelectAllEtcItemData`), ~100 `OnResult*` handler; parser `ioUserEtcItem.cpp:144`, `ioUserExtraItem.cpp:587`, `ioInventory.cpp` (`DBtoData`)
- **Tipe:** Untrusted-input trust boundary violation
- **Root cause:** Result handler percaya apa pun yang DB Agent kirim. `query_data->GetValue(...)` hanya cek vs `nResultBufferSize` (wire-claimed size, DB-1), bukan signature/authenticity. Dengan bad bounds DB-1, network attacker supply result arbitrary yang rows decode ke `m_money`, `m_user_state`, item count, char data attacker-chosen. Handler apply ke live `User` (mis. `OnResultSelectUserData` tulis `user_data.m_money` dari wire).
- **PoC:** Same access DB-1. Inject `DBAGENT_USER_DATA_GET` result dgn `m_money=INT64_MAX`, `m_user_state=normal`, fresh `nick_name`/`szDBID` match victim di flow `OnResultSelectUserData` → money/state victim overwrite dari forged packet.
- **Fix:** Authenticate/integrity DB-Agent link (MAC shared secret); harden semua `OnResult*` validate value range (money ≥0, index dalam tabel, count ≤ hard cap) sebelum apply.

### DB-3 — MEDIUM: Unbounded `while(IsExist())` parse loop (no loop cap)
- **Lokasi:** `ioUserEtcItem.cpp:144-159`, `ioUserExtraItem.cpp:587-616`, `ioInventory.cpp` (`DBtoData`), `CardMatching.cpp:296`, `DiceGame.cpp:73`, `GuildRoomInfos.cpp:35`
- **Root cause:** Loop gated `query_data->IsExist()` (`QueryResultData.cpp:178-183` = `m_nValuePos < nResultBufferSize`). `LOOP_GUARD()` (`MainProcess.h:384-385`) hanya record file/line untuk *post-mortem* crash logging — **bukan** iterasi cap (`MainProcess.cpp:986-990` cuma store string). Rows `push_back` ke `std::vector` tanpa count limit. `nResultBufferSize` attacker-controlled (DB-1).
- **PoC:** Forged result `nResultBufferSize ≈ 64000` dgn shape 1-byte-per-iteration (each `GetValue` fail fast via `PACKET_GUARD_BREAK` return sebelum push — tapi loop tetap spin) → CPU burn; atau shape yang legit append `ETCITEMDB` (~124 B) → ~500+ entri per query → repeated banyak user → memory amplification.
- **Fix:** Replace `LOOP_GUARD()` dgn real counter (`for(int i=0; i<GetResultCount() && query_data->IsExist(); i++)` + hard cap). Validate `GetResultCount()` vs `nResultBufferSize / minRowSize`.

### DB-4 — HIGH (SUSPECTED): SQL injection via inconsistent text escaping
- **Lokasi:** `DBClient.cpp:4216-4217` (`OnSelectCreateGuild`, `szGuildName`/`szGuildTitle` **unescaped**), `:4721` (`OnSelectGuildNameChange`, `szGuildName` unescaped), `:5413-5414` (`OnInsertPresentData`, `szSendName`/`szRecvName` unescaped), `:5464-5465`/`:5501` (`OnInsertPresentDataLog`, `OnPresentInsertByPrivateID`), `:5934-5935` (`OnUpdatePublicIDAndEtcItem`, `rszPublicID`/`rszNewPublicID` unescaped), `:6118`/`:6155` (nickname). Contrast escaped: `:3950-3952` (`OnInsertTrial`), `:4678` (`OnUpdateGuildTitle`).
- **Root cause:** Hanya chat/reason/guild-*title* lewat `Help::GetSafeTextWriteDB` (`EtcHelpFunc.cpp:535-568`, double `'` + HTML-escape `<>`). Guild *name*, present send/recv *name*, changed nickname pass raw via `v_FT.Write(str, len, TRUE)` (`cSerialize.h:57-62`, length-prefixed tapi **not** SQL-escaped). Jika DB Agent concat ke `exec proc N, '<name>'` → `'` break out.
- **PoC:** Player create guild `a' OR 1=1;--` (length 14 ≥4, DBCS-clean, lewat `ioMainProcess::IsRightID` `MainProcess.cpp:1612-1629` yang cuma cek length + lead-byte). Jika locale `IsRightNewID` = default `ioLocalParent::IsRightNewID` (return true, `ioLocalParent.h:75`) atau permissive override, & `sp2_not_make_id.ini` tidak blacklist `'`/`;`/`-` → name reach DB Agent unescaped → SQL injection. (Indonesia `IsRightNewID` alnum-only block — `ioLocalIndonesia.cpp:255-267` — locale matters.)
- **Fix:** Escape **semua** text bound DB dgn `GetSafeTextWriteDB` (atau lebih baik DB Agent parameter-bind). Apply uniform server-side char whitelist (`[A-Za-z0-9]` + locale multi-byte) ke **semua** name field regardless locale.

### DB-5 — MEDIUM: Weak/inconsistent server-side name validation (enabler DB-4)
- **Lokasi:** `MainProcess.cpp:1612-1629` (`IsRightID`: length + DBCS only), `:1127-1208` (`IsNotMakeID`: config blacklist lower-cased), `Filter/WordFilterManager.cpp:24-40` (`IsSpecialLetters` hanya block `\n` — entry tunggal `'\n'` `:26`), `Local/ioLocalParent.h:75` (default `IsRightNewID` return true)
- **Root cause:** "Special letter" filter efektif newline-only check. Defense real cuma locale-specific (`ioLocalIndonesia::IsRightNewID` alnum-only; base accept everything). `IsNotMakeID` substring blacklist dari `sp2_not_make_id.ini` — brittle, bypassable, lower-cased (tidak match DBCS rule consistent). Guild-name creation (`ioEtcItem.cpp:1423-1511`) call `IsRightID`+`IsNotMakeID`+`IsRightNewID` tapi **tidak** `CheckSpecialLetters`.
- **PoC:** Region dgn `IsRightNewID` default (return true) & `sp2_not_make_id.ini` tidak list `'`,`;`,`--`,`(`: player kirim `CTPK` guild-create `szGuildName = "x'; EXEC sp_addrole 'a';--"` → lewat `IsRightID` (length≥4, ASCII, DBCS-clean), `IsNotMakeID` (jika tidak blacklist), `IsRightNewID` (true). Name flow unescaped ke DB.
- **Fix:** Single global whitelist (`isalnum` + locale DBCS) untuk semua user identifier di packet-handler entry, independent locale override. `IsSpecialLetters` enumerate SQL/HTML metachar actual.

### DB-6 — LOW (latent): `cSerialize::Write(str, len, TRUE)` truncate length ke `uint16`
- **Lokasi:** `Util/cSerialize.h:51-62`
- **Root cause:** `if(saveLength) Write(static_cast<uint16>(length));` lalu `m_storage.Append(value, length)`. `length > 65535` → wrapped 16-bit length prefix tapi full payload → DB Agent (read prefix) mis-size field.
- **PoC:** Tidak reachable saat ini (semua caller bounded `ioHashString::Length()` ≤ `ID_NUM_PLUS_ONE`=41, `GUILD_NAME_NUM_PLUS_ONE`=21, dll).
- **Fix:** Assert `length <= UINT16_MAX` saat `saveLength==TRUE`, atau widen prefix ke `uint32`.

### DB-7 — LOW (latent): `CQueryData::SetReturnData` write fixed `m_szReturnBuf[MAX_BUFFER]` no bound enforcement
- **Lokasi:** `QueryData/QueryData.cpp:69-76`
- **Root cause:** `memcpy(&m_szReturnBuf[m_iReturnLength], pData, iSize); m_iReturnLength += iSize; if(m_iReturnLength >= MAX_BUFFER) LOG(...)` — overflow check **setelah** memcpy & hanya log; tidak block. `m_szReturnBuf = char[MAX_BUFFER]`. 540 call site; largest total per query ≪ 64KB → tidak trigger hari ini, tapi single over-long/repeated call overflow.
- **Fix:** Bound sebelum copy: `if(m_iReturnLength + iSize > MAX_BUFFER) return;` (atau clamp); caller honor bool/return.

### DB-8 — LOW (latent): `CQueryData::GetResults`/`GetFields`/`GetReturns` no bounds vs allocation
- **Lokasi:** `QueryData/QueryData.cpp:27-57` (`GetFields`, `GetResults`), `:59-67` (`GetReturns`)
- **Root cause:** `GetResults` loop `n < GetResultSize()` step `sizeof(ValueType)`, cast `(ValueType*)(m_pBuffer + index + n)`. `index = sizeof(int) + GetFieldSize()`. Jika `nFieldLength + nResultLength` exceed actual `nQueryBufferSize` alloc untuk `m_pBuffer` → read past buffer. `GetReturns` `memcpy(buffer, m_pBuffer + index, GetReturnSize())` tanpa cek.
- **Reachability:** `CQueryData` deserialize dari wire hanya di `LogDBClient::SendMessage` (`LogDBClient.cpp:138-141`) di path **send-failure** vs packet yang server sendiri construct — trusted. `>> CQueryData` (`SP2Packet.cpp:663-676`) punya bug pre-`SetBuffer` size-check sama DB-1, tapi input self-generated. LOW; HIGH jika `CQueryData` pernah dari untrusted source.
- **Fix:** `GetResults/GetFields/GetReturns` validate offset vs `GetBufferSize()` sebelum read; `SetBuffer` (`QueryData.cpp:78-84`) reject `nQueryBufferSize < 0` atau `> MAX_BUFFER`.

### DB-9 — MEDIUM (SUSPECTED): Async DB result → user lookup null-checked tapi not racefree vs teardown (TOCTOU UAF)
- **Lokasi:** `UserNodeManager.cpp:3341-3393` (`OnResultSelectUserData`), `3897-3919` (`OnResultLoginSelectAllEtcItemData`), `4373-4414` (`OnResultLoginSelectAllInvenData`), semua `OnResult*` family
- **Root cause:** Tiap handler `User *pUser = GetUserNode(dwUserIdx); if(pUser==NULL) return;` lalu deref `pUser` berulang, sering fire **lebih** DB query yang result-nya re-lookup user sama. Tidak ada refcount/lock hold di `pUser` antara lookup & use. Jika IOCP worker thread yang proses user disconnect & thread yang proses `g_RecvQueue` (`PK_QUEUE_QUERY`, insert `DBClient.cpp:93`) beda → disconnect antara null-check & `pUser->...` use → free node di handler → UAF. (Tiap handler re-check mitigasi cross-handler UAF, tapi tidak intra-handler TOCTOU.)
- **PoC:** Trigger butuh DB query in-flight untuk user yang disconnect (mis. close socket saat login data load). UAF tergantung thread model. Flag untuk dynamic analysis.
- **Fix:** Hold shared/refcounted pointer (atau per-user query "in-flight" token) across handler, atau guarantee single-threaded processing user teardown + DB result.

### DB-10 — MEDIUM: DB-Agent disconnect fatal ke whole game server; no in-flight query reconciliation
- **Lokasi:** `DBClient.cpp:77-86` (`SessionClose`), `407-413` (`OnCloseDBAgent` → `g_App.Shutdown(SHUTDOWN_DBAGENT, 3)`), `345-369` (`SendMessage` falls through ke random agent on miss)
- **Root cause:** Setiap DB-Agent socket close trigger `SHUTDOWN_DBAGENT` → full game-server shutdown. Tidak ada per-query tracking table, retry, queue in-flight; mereka hilang. `SendMessage` ke missing/inactive agent ID silently re-send ke **agent acak** (`DBClient.cpp:360-367`) → partial agent outage mis-route user query ke wrong DB shard (nIndex/DBID mismatch) → logic corruption risk + shutdown.
- **PoC:** Attacker RST/flood DB-Agent TCP (atau crash DB Agent) → force game server shutdown semua player. RST berulang → outage persistent.
- **Fix:** Distinguish transient socket error vs agent death; jangan shutdown single agent failure jika agent lain live. Jangan fallback random agent untuk specific `dwAgentServerID` (return failure). Track in-flight query & re-issue/explicit fail.

### DB-11 — MEDIUM: Unauthenticated plaintext DB-Agent link
- **Lokasi:** `DBClient.cpp:67-70` (`CheckNS` return true), `221-267` (`ConnectTo` plain `connect()`), `473-511` (DBAgentNode def)
- **Root cause:** Game server ↔ DB Agent TCP no auth, no integrity, no TLS. Host mana saja yang reach DB Agent port (atau ARP/DNS-spoof internal link) inject `DTPK_QUERY` result → trigger DB-1, DB-2, DB-10.
- **Fix:** TLS atau per-packet HMAC shared secret keyed per server instance.

### DB-12 — LOW: Dead SQL-string building (code-hygiene / latent regression)
- **Lokasi:** `DBClient.cpp:2699,2772,2971,3006,4173-4174,5925-5926,5966,6111,6147` (+ ~230 commented `sprintf_s`/`wsprintf`)
- **Root cause:** Banyak `OnSelect/OnInsert` masih compute `char str_query[MAX_QUERY_SIZE]; sprintf_s(str_query, "exec ... %d", idx);` tapi tidak pernah kirim `str_query` — real query via `cSerialize v_FT`. Vestigial obscure data flow, risk uncommented/repurposed, `MAX_QUERY_SIZE`=2048 (`DBClient.h:464`) sementara active build pakai user data indirect → future edit silently reintroduce string-concat injection.
- **Fix:** Delete vestigial `str_query` block.

### DB-13 — LOW (SUSPECTED): Pagination loop drivable indefinitely by malicious DB Agent
- **Lokasi:** `UserNodeManager.cpp:4397-4413` (`OnResultLoginSelectAllInvenData`), `3989-4026` (`OnResultLoginSelectAllExtraItemData`), paged loader (`DB_*_SELECT_COUNT`)
- **Root cause:** `iDBSelectCount = GetResultCount(); ... if(iDBSelectCount < DB_*_SELECT_COUNT) { finish } else { re-query iLastIndex }`. Malicious DB Agent return `count >= threshold` dgn `iLastIndex` unchanged (atau oscillating) → loader re-issue `OnLoginSelectAllInvenData` indefinite → hammer DB Agent & stall user login forever.
- **Fix:** Cap pagination depth per user (mis. max 10 page), abort login on cap exceeded.

---

## Prioritas Fix Lokal
1. **DB-1 (P1):** Validate `nResultBufferSize` vs remaining packet sebelum `new`.
2. **DB-2 (P1):** Authenticate DB-Agent link + validate value range di `OnResult*`.
3. **DB-11 (P0):** TLS/HMAC DB-Agent link.
4. **DB-4 (P1):** Escape konsisten + whitelist name.
5. **DB-3 (P2):** Real loop cap.
6. **DB-9/DB-10 (P2):** Lifetime + reconnect robustness.
