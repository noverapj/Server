# 07 — BattleRoom / Game Modes

**Area:** Logic gameplay, state machine match, validasi index/slot, integritas hasil.
**File dibaca:** `BattleRoomNode.cpp/.h`, `BattleRoomManager.cpp/.h`, `BattleRoomParent.cpp/.h`, `BattleRoomReserveMgr.cpp/.h`, `Room.cpp/.h`, `RoomParent.cpp/.h`, `Mode.cpp/.h`, `ModeSelectManager.cpp/.h`, `ModeCreator.cpp/.h`, `MatchManager.cpp/.h`, `ChannelNode.cpp/.h`, `LadderTeamNode.cpp/.h`, `CatchMode.cpp/.h`

---

## Catatan Arsitektur (enable mayoritas temuan)
Routing gameplay packet **tidak** ada observer/state gate di dispatcher:
- `User::PacketParsing` call `OnRoomProcessPacket` untuk **setiap** TCP packet sebelum per-packet switch (`User.cpp:8081`).
- `Room::ProcessTCPPacket` handle core gameplay (DROP_DIE, WEAPON_DIE, PASSAGE, dll) **tanpa observer check**, lalu forward ke `m_pMode->ProcessTCPPacket` (`Room.cpp:2907-2947`).
- `Mode::ProcessTCPPacket` dispatch damage/contribute/award/prisoner/catch/machine **tanpa match-state check** (`Mode.cpp:3746-3840`).
- `Room::AddUser` selalu add user ke `m_vUserNode` & create `ModeRecord` (`Room.cpp:1682-1700`); `BattleEnterRoom` call `EnterRoom` untuk **semua** `m_vUserNode` termasuk observer (`BattleRoomNode.cpp:850-895`).
- `Mode::IsEnableState` return **true untuk observer** (`RS_OBSERVER != RS_LOADING`) (`Mode.cpp:6092-6105`).

Konsekuensi: observer/viewer/late-joiner bisa panggil hampir semua handler gameplay, & dispatcher tidak enforce "match in MS_PLAY". Ini compound setiap temuan individu di bawah.

---

## CRITICAL

### G-1 — Match-result/award manipulation via client-supplied awarding list
- **Lokasi:** `Mode.cpp:5747` (`Mode::OnAwardingResult`, dispatch `Mode.cpp:3789`)
- **Root cause:** `CTPK_AWARDING_RESULT` accept dari client in-room mana saja. Gate tunggal: one-shot `m_bCheckAwardChoose` flag. **Tidak** cek match `MS_RESULT`/`MS_RESULT_WAIT`, tidak host/owner check, server trust client-supplied list ≤50 `(iType, szName, iValue)` tuple. Tiap entri server call `pRecord->pUser->AddAward(iType, iPoint)` (`ioAward.cpp:208`, persist DB `m_bChange=true`) **&** `g_PresentHelper.SendAwardEtcItemBonus(...)` (actual item grant).
- **PoC:** Selama match apa pun (bahkan mid-MS_PLAY, asal `m_dwModePointTime != 0`), kirim `CTPK_AWARDING_RESULT` dgn `iAwardSize=50` & 50 entri semua nama ID sendiri dgn `iType` varied. Terima 50 award record + 50 etc-item bonus. Karena set `m_bCheckAwardChoose=true` pertama, awarding legit berikutnya suppressed untuk orang lain.
- **Fix:** Compute award list server-side dari `ModeRecord`; reject `CTPK_AWARDING_RESULT` kecuali `GetState()==MS_RESULT_WAIT`; require dari mode/server bukan client; bound & whitelist `iType`.

### G-2 — Match-win fabrication di tournament sudden-death via client-supplied contribute
- **Lokasi:** `Mode.cpp:5476` (`Mode::OnSuddenDeathPlayRecordInfo`), winner decided `:5576-5602`
- **Root cause:** Tournament sudden-death: **winner decided by compare `fBlueContributePer` vs `fRedContributePer`**, keduanya compute entirely dari client-supplied `iContribute` (`Mode.cpp:5512-5573`). Handler punya state guard (`m_bTournamentRoom`, `MS_PLAY`, one-shot flag) tapi **tidak** validate contribute plausible atau sender authoritative. First sender wins.
- **PoC:** Tournament sudden-death round, jadi first kirim `CTPK_TOURNAMENT_SUDDEN_DEATH` dgn `TOURNAMENT_SUDDEN_DEATH_CONTRIBUTE`, `iCharCnt` = number of record, assign `iContribute` huge ke semua anggota tim & 0 ke lawan. Tim dinyatakan winner → tournament advancement.
- **Fix:** Server track contribute/kill/death sendiri dari authoritative gameplay event; jangan accept total dari client.

### G-3 — Per-match contribute/kill/death record fully client-controlled
- **Lokasi:** `Mode.cpp:5375` (`Mode::OnLastPlayRecordInfo`, dispatch `:3784`)
- **Root cause:** `CTPK_LAST_PLAYRECORD_INFO` accept dari client in-room mana saja. Gate tunggal one-shot `m_bCheckContribute`. Tiap entri ≤50, server store client-supplied `iContribute/iUniqueTotalKill/iUniqueTotalDeath/iVictories` langsung ke `ModeRecord` user bernama (`Mode.cpp:5399-5402`) & recompute `fContributePer` broadcast ke semua client. Tidak state check, tidak sender-authority check.
- **PoC:** Kirim `CTPK_LAST_PLAYRECORD_INFO` pertama dgn nama sendiri `iContribute=INT_MAX`, kills=999, deaths=0, victories=999. Jadi MVP, inflate contribute %, value feed downstream award/anti-abuse logic.
- **Fix:** Reject; compute server-side. Minimal bound value & verify sender==host & state==MS_RESULT_WAIT.

### G-4 — Client-supplied damage total (anti-abuse bypass + opponent suppression)
- **Lokasi:** `Mode.cpp:4355` (`Mode::OnCurrentDamageList`, dispatch `:3765`)
- **Root cause:** Client kirim `iDamageUserCnt` (cap 500) & `(szName, iTotalDamage)` pair; server `pRecord->iTotalDamage += iTotalDamage` untuk user bernama **apa pun** tanpa validasi sign/magnitude/ownership/match state. `iTotalDamage` = signed `int`.
- **PoC:** (a) Set `iTotalDamage` sendiri huge untuk lewat abuse gate `pRecord->iTotalDamage <= m_iAbuseMinDamage` di `CheckSpacialAwardUser` (`Mode.cpp:5684`). (b) Kirim damage entry negatif/0 untuk lawan supaya mereka fail gate sama & kalah special-award eligibility. Integer overflow via `INT_MAX` damage possible.
- **Fix:** Validate `iTotalDamage >= 0`, bound, hanya credit damage ke user yang sender berhak report, require `MS_PLAY`.

### G-5 — Arbitrary-recipient kill/death injection (DropDie/WeaponDie)
- **Lokasi:** `Room.cpp:2949` (`Room::OnDropDie`), `:2968` (`Room::OnWeaponDie`)
- **Root cause:** Keduanya read `szDieChar` (nama yang mati) dari client & call `OnDropDieUser/OnWeaponDieUser` untuk user itu. **Tidak** cek `pSend` benar-benar kill `szDieChar`, `szDieChar` di team lawan, `szDieChar` alive, atau match `MS_PLAY`. Di FlagMode, `FlagModeAddPoint(pRecord->pUser->GetTeam(), pSend)` credit pSend sebagai killer & add point ke team user yang mati (`Room.cpp:2984`).
- **PoC:** Kirim `CTPK_WEAPON_DIE` berulang dgn `szDieChar` = nama tiap lawan → di-credit sebagai killer, lawan forced dead, tim farm point/kill ke MVP & match win. Observer juga bisa.
- **Fix:** Validate sender↔victim relationship server-side dari authoritative hit/death event; jangan biarkan client name siapa yang mati.

### G-6 — Prisoner-state injection + counter corruption (Catch mode)
- **Lokasi:** `CatchMode.cpp:849` (`CatchMode::OnPrisonerMode`), `:740` (`OnPrisonerEscape`), `:774` (`OnPrisonerDrop`)
- **Root cause:** `CTPK_PRISONERMODE`: client supply `szPrisoner` (victim) & `szLastAttacker`. Tidak cek `pSend` adalah captor, `szPrisoner` di team lawan, atau match `MS_PLAY` (`bPrisoner=true` & `m_iBlueCatchRedPlayer++/m_iRedCatchBluePlayer++` happen **sebelum** `GetState()==MS_PLAY` check `CatchMode.cpp:907`). Counter increment pakai `pUser->GetTeam()`, jadi player red capture red teammate increment `m_iBlueCatchRedPlayer` → korup prisoner-count yang dipakai untuk win condition. `CTPK_PRISONER_ESCAPE`: client mana saja free prisoner mana saja (`szName` lookup, no ownership check). Damage di `OnPrisonerDrop`/`OnPrisonerMode` (`iDamageCnt` loop) add langsung ke `pRecord->iTotalDamage` dari client value.
- **PoC:** Kirim `CTPK_PRISONERMODE` dgn `szPrisoner`=lawan mana saja (atau teammate) untuk force prisoner & manipulasi win counter; kirim `CTPK_PRISONER_ESCAPE` dgn nama teammate untuk instant free.
- **Fix:** Validate captor di team lawan prisoner, prisoner alive/armed, state `MS_PLAY`, compute capture server-side.

### G-7 — Arbitrary item spawn / self-equip by item code
- **Lokasi:** `Mode.cpp:3955` (`Mode::OnCreateObjectItem`, dispatch `:3759`)
- **Root cause:** Client supply `dwItemCode`; server call `m_pCreator->CreateItemByCode(dwItemCode)` (`Room.cpp:2335`, delegate ke global item DB tanpa ownership/spawn-limit/skill check) & equip langsung ke `pSend` (`ImmediatelyEquipItem`, `Mode.cpp:3982`) atau add ke field untuk siapa saja pick (`AddFieldItem`, `Mode.cpp:3991`). `CTPK_CREATE_FIELD_ITEM_BY_USE_SKILL` (`Mode.cpp:4007`) sama, client trigger gashapon package lookup.
- **PoC:** Mid-match kirim `CTPK_CREATE_OBJECTITEM` dgn `dwItemCode` weapon powerful/limited & `bImmediately=true` → equip item tidak punya. Atau `bImmediately=false` spawn untuk accomplice pick.
- **Fix:** Whitelist spawnable item code per skill/mode; verify sender hold spawning skill/item; jangan biarkan client pick item code.

---

## HIGH

### G-H1 — Any member start/restart battle (no owner gate, non-idempotent)
- **Lokasi:** `BattleRoomNode.cpp:3469` (BATTLEROOM_READY_GO) → `OnBattleRoomReadyGO` `:3652`
- **Root cause:** Beda `BATTLEROOM_MODE_SEL` (owner-check `:3403`) & `BATTLEROOM_TEAM_CHANGE`, `BATTLEROOM_READY_GO` **tidak** cek `GetOwnerName()==pUser->GetPublicID()` & tidak `bCommandBlock`. Member mana saja bisa start. Lebih parah, branch pertama (`m_pBattleRoom && !IsRoomEmpty()`) re-run `SetModeType`/`CreateNextShamBattle` di **live** room → restart/ubah mode mid-match.
- **PoC:** Join room; saat match berjalan, kirim `CTPK_BATTLEROOM_COMMAND` `BATTLEROOM_READY_GO` → match in-progress di-re-init.
- **Fix:** Gate `BATTLEROOM_READY_GO` di owner (atau host) & state "ready"; idempotent.

### G-H2 — Unbounded team/observer capacity (no upper-bound `SetMaxPlayer`/room create)
- **Lokasi:** `BattleRoomNode.cpp:1877` (`SetMaxPlayer`); `User.cpp:12799`/`12832` (`OnCreateBattleRoom`); `User.cpp:11437` (`PLAZA_CMD_MAXPLAYER_CHANGE`)
- **Root cause:** `iBluePlayer/iRedPlayer/iObserver` dari client ditulis ke `m_iMaxPlayerBlue/Red/Observer` **tanpa** upper-bound check vs `MAX_BATTLEROOM_PLAYER (16)` atau limit apapun. `AddUser`/`EnterUser` tidak ada capacity check independen. Floor clamp tunggal "tidak shrink below current team count". Negatif diterima.
- **PoC:** Create battle room `iBluePlayer=INT_MAX`; `IsFull()` (`:2394`) tidak pernah true → unlimited player join 1 room (memory/packet-fanout DoS). Set `iBluePlayer=-1` → `IsFull()` always true (lock room).
- **Fix:** Clamp ketiga ke `[0, MAX_BATTLEROOM_PLAYER]` di `SetMaxPlayer`.

### G-H3 — Observer/spectator bisa invoke semua gameplay handler (state bypass)
- **Lokasi:** `Mode.cpp:6092` (`IsEnableState`), `Room.cpp:2907` (`ProcessTCPPacket`), `User.cpp:8081` (routing), `Room.cpp:1682` (`AddUser`)
- **Root cause:** Observer dapat `ModeRecord` `eState=RS_OBSERVER` (`Mode.cpp:4436`) & `IsEnableState` return true; dispatcher tidak filter observer. Observer lewat `IsEnableState` guard di `OnCreateObjectItem`, `OnPushStructCreate`, `OnMachineStructTake/Die/Release`, `OnAbsorbInfo`, `OnChatMode`, dll, & bisa kirim `CTPK_DROP_DIE`/`WEAPON_DIE`/`PRISONERMODE`/`CURRENT_DAMAGELIST`/`CATCH_CHAR`.
- **PoC:** Join sebagai observer; kirim `CTPK_CREATE_OBJECTITEM`, `CTPK_PUSHSTRUCT_CREATE`, `CTPK_CURRENT_DAMAGELIST`, `CTPK_PRISONERMODE`, dll — semua diproses.
- **Fix:** Observer check di dispatcher &/atau tiap gameplay handler (reject jika `pSend->IsObserver()` atau `pRecord->eState != RS_PLAY` untuk state-mutating packet).

### G-H4 — Ladder ranking farmable via client-controlled match inputs + accomplice (SUSPECTED)
- **Lokasi:** `Mode.cpp:5375` (contribute), `:4355` (damage), `Room.cpp:3032` (`OnLadderBattleRestart`)
- **Root cause:** Ladder match outcome derive dari contribute/damage/kill record client-supplied (G-3/G-4). Dua tim ladder komplice queue ke satu sama lain & "winner" supply fabricated contribute untuk guarantee win, lalu `OnLadderBattleRestart` (`bRestart=true`) re-queue instant. Tidak server-side integrity check result.
- **PoC:** 2 tim komplice match; designated winner kirim `CTPK_LAST_PLAYRECORD_INFO`/`CTPK_TOURNAMENT_SUDDEN_DEATH` dgn inflated contribute → guaranteed win → rank farm.
- **Fix:** Compute match outcome server-side dari authoritative event; rate-limit rematch antar tim sama; detect collusion.

### G-H5 — Unbounded push-struct list growth (memory exhaustion)
- **Lokasi:** `Mode.cpp:4241` (`OnPushStructCreate`), unbounded `push_back` `:4327`; `m_iPushStructIdx++` `:4322`
- **Root cause:** Tidak cap `m_vPushStructList.size()`. Tiap `CTPK_PUSHSTRUCT_CREATE` dgn `iDeleteIndex` yang match no existing struct (common case) hanya add, tidak remove. Hanya branch compile-time `FISHING_SYSTEM_EX` punya ownership/duplicate guard.
- **PoC:** Spam `CTPK_PUSHSTRUCT_CREATE` dgn `iDeleteIndex` = index never-used → `m_vPushStructList` grow tanpa bound → server memory exhaustion.
- **Fix:** Cap list size per room/mode; require `iDeleteIndex` match owned struct; rate-limit.

### G-H6 — Targeted client-to-client packet injection (OnPassage) + arbitrary raw relay
- **Lokasi:** `Room.cpp:3000` (`OnPassage`); raw relay juga `Mode.cpp:5291/5309` (`OnCatchChar`), `:5618` (`OnAwardingInfo`), `:5371` (`OnPlayRecordInfo`)
- **Root cause:** `OnPassage` read `szTargetUser` dari packet & forward **remaining raw bytes** (`SetDataAdd(rkPacket.GetData(), rkPacket.GetDataSize())`) ke user spesifik itu. Client mana saja bisa kirim arbitrary data ke client in-room lain, & `OnCatchChar`/`OnAwardingInfo`/`OnPlayRecordInfo` sama relay attacker-controlled bytes ke semua/selected client.
- **PoC:** Kirim `CTPK_PASSAGE` dgn `szTargetUser`=victim & crafted payload → client victim parse attacker-controlled field (potential client-side crash/state corrupt).
- **Fix:** Jangan relay raw client bytes; re-serialize dgn server-validated field.

---

## MEDIUM

### G-M1 — Plaza room takeover (name/password/max-player) tanpa master check
- **Lokasi:** `User.cpp:11422` (`PLAZA_CMD_NAME_PW_CHANGE`), `:11434` (`PLAZA_CMD_MAXPLAYER_CHANGE`)
- **Root cause:** `OnPlazaCommand` hanya cek `m_pMyRoom && MT_TRAINING`; **tidak** cek `GetMasterName()==GetPublicID()` (contrast `PLAZA_CMD_KICK_OUT` `:11447` yang cek). Member mana saja bisa set password open plaza (lock orang lain) & ubah max player ke arbitrary (G-H2).
- **Fix:** Apply master check sama `PLAZA_CMD_KICK_OUT`.

### G-M2 — Invalid `TeamType` accepted (phantom-team state)
- **Lokasi:** `BattleRoomNode.cpp:3341` (BATTLEROOM_TEAM_CHANGE), `:1018` (`IsBattleTeamChangeOK`), `:2109` (`ChangeTeamType`)
- **Root cause:** `iTeamType` read `int` & cast `TeamType` tanpa validation. `IsBattleTeamChangeOK` hanya special-case `TEAM_BLUE/RED/NONE`; value lain tidak match branch capacity → fall-through `return true`. `ChangeTeamType` store invalid team. User jadi play user (`m_bObserver` false) dihitung tidak blue tidak red, bypass team-balance/limit.
- **PoC:** Kirim `CTPK_BATTLEROOM_COMMAND` `BATTLEROOM_TEAM_CHANGE` `iTeamType=99` saat non-observer → teamless-playing, immune team-full check.
- **Fix:** Validate `iTeamType ∈ {TEAM_NONE, TEAM_RED, TEAM_BLUE}` sebelum process.

### G-M3 — Non-owner member invite arbitrary user ke battle room
- **Lokasi:** `BattleRoomNode.cpp:3213` (`OnBattleRoomInvite`)
- **Root cause:** Tidak cek `GetOwnerName()==pUser->GetPublicID()`; member mana saja kirim `CTPK_BATTLEROOM_INVITE` & push invite popup ke user mana saja (`g_UserNodeManager.GetGlobalUserNode(szInvitedID)`). `iSize` cap 50 via `MAX_GUARD`.
- **Fix:** Restrict invite ke owner (atau pakai invite-pool mechanism konsisten).

### G-M4 — Catch-char / escape state toggle di player mana saja
- **Lokasi:** `Mode.cpp:5267` (`OnCatchChar`), `:5325` (`OnEscapeCatchChar`)
- **Root cause:** Client supply `szName` target; server set `pRecord->bCatchState` true/false untuk user itu tanpa cek `pSend` adalah binder atau target valid. Hanya `RS_LOADING` excluded.
- **Fix:** Validate bind relationship server-side.

### G-M5 — Machine-struct destroy/take oleh client mana saja (termasuk observer)
- **Lokasi:** `Mode.cpp:6995` (`OnMachineStructDie`), `:7021`, `:7059`
- **Root cause:** `iIndex` lookup only (no OOB), tapi client in-match mana saja (observer lewat `IsEnableState`) destroy/take/release machine struct by index. `OnMachineStructDie` destroy tanpa ownership check.
- **Fix:** Restrict ke owning/taking user & `MS_PLAY`.

### G-M6 — Ladder battle restart tanpa match-end check (SUSPECTED)
- **Lokasi:** `Room.cpp:3032` (`OnLadderBattleRestart`)
- **Root cause:** `bRestart` honored hanya dgn `GetRoomStyle()==RSTYLE_LADDERBATTLE` & kondisi "both teams restarted"; tidak cek match/round current sudah end. Bisa force-restart match kalah atau skip result.
- **Fix:** Hanya accept restart setelah match reach terminal state.

### G-M7 — `iDamageCnt`-driven unbounded allocation di prisoner handler
- **Lokasi:** `CatchMode.cpp:786` (`OnPrisonerDrop`), `:875` (`OnPrisonerMode`); `Mode.cpp:4359` (`OnCurrentDamageList`), `:4078/4088/4104` (`OnAbsorbInfo`), `:5384/5497`
- **Root cause:** `vDamageList.reserve(iDamageCnt)`/`reserve(iMineCnt)` pakai client-supplied count. `OnPrisonerDrop`/`OnPrisonerMode` **tidak** `MAX_GUARD` `iDamageCnt` (beda `OnCurrentDamageList` cap 500 & `OnAbsorbInfo` cap 100). Huge `iDamageCnt` → huge `reserve` → `bad_alloc`/OOM.
- **Fix:** `MAX_GUARD(iDamageCnt, <small>)` di prisoner handler; bound semua client-driven `reserve()` count.

### G-M8 — `GetUserNodeByIndex` return shared static reference (SUSPECTED)
- **Lokasi:** `BattleRoomNode.cpp:2484`
- **Root cause:** Saat `dwUserIndex` tidak ketemu, return `static BattleRoomUser kReturn`. Caller yang write via returned reference mutasi process-global object shared semua lookup, & non-const reference return. `GetUserNodeByArray` (`:2499`) & lain pakai pattern sama. Caller seperti `UserP2PRelayInfo` (`:601/604`) take reference & mutasi (`kUser.m_iServerRelayCount++`) — jika user tidak ketemu mereka mutasi static, corrupt "not found" result berikut & (untuk relay) increment shared counter.
- **Fix:** Return pointer (NULL on miss) atau non-static temporary; jangan mutasi via not-found sentinel.

---

## LOW

### G-L1 — `OnCharSlotChange` MT_ARENA branch bypass index validation
- **Lokasi:** `User.cpp:10811-10820`
- Branch ARENA return sebelum `COMPARE` bounds check. Hanya echo index (no indexing) → no OOB, tapi accept unbounded index & report success — minor logic inconsistency.

### G-L2 — `BATTLEROOM_RUNNER_SELECT` order value unvalidated
- **Lokasi:** `BattleRoomNode.cpp:3480-3502`
- `iOrderNum` store ke `m_eSelect_Order` & broadcast tanpa range check. Logic-only impact.

### G-L3 — Ladder `LADDERTEAM_MACRO_MODE_SEL` mode/map index unbounded
- **Lokasi:** `LadderTeamNode.cpp:1703`
- Owner-gated, tapi `m_iSelectMode/m_iSelectMap` set dari packet tanpa validation; bisa select invalid mode. Low.

### G-L4 — `BattleEnterRoom` orphan-on-recreate
- **Lokasi:** `BattleRoomNode.cpp:850`
- `m_pBattleRoom = pRoom` overwrite pointer prior tanpa release. Room owned `g_RoomNodeManager` (reaped saat empty) jadi bukan hard leak, tapi hazard jika ReadyGO kedua saat prior room non-empty (mitigated oleh first-branch check `OnBattleRoomReadyGO`, tapi G-H1 bypass itu).

### G-L5 — `OnAbstract` dead-code stack buffer read
- **Lokasi:** `User.cpp:12504-12565`
- `char sztemp[32768]; packet >> sztemp;` = stack BOF, tapi dalam `#if 0` (dead). Live `OnAbstract` (`:12567+`) no-op. Untuk kelengkapan.

### G-L6 — `ioHashString` packet read rely `lstrlen` over packet buffer (HIGH via breadth)
- **Lokasi:** `SP2Packet.cpp:619` (`operator>>(ioHashString&)`)
- `nlen = lstrlen((LPTSTR)&m_pBuffer[m_currentPos])` compute **sebelum** `CheckRightPacketSize(nlen)`. Malformed packet string field tanpa NUL terminator sebelum buffer end → `lstrlen` read past allocation (heap OOB read → crash/info leak). Pengaruhi **setiap** gameplay handler baca nama (`OnDropDie`, `OnWeaponDie`, `OnPassage`, `OnPrisonerMode`, `OnCatchChar`, `OnBattleRoomInvite`, `OnPlazaCommand`, dll). LOW mark hanya karena already-confirmed root cause; reachability via gameplay packet = HIGH.
- **Fix:** Read dgn explicit length/bounded scan; cap string field; reject packet yang NUL tidak found dalam remaining size.

---

## Memory / Lifecycle
- `Room::~Room` (`Room.cpp:153`) & `Room::OnDestroy` (`:190`) keduanya call `DestroyMode()` (`:1773`). **Aman**: `DestroyMode` pakai `SAFEDELETE(m_pMode)` yang null pointer → call kedua no-op (tidak double-free).
- `BattleRoomManager` pakai mempool (`BattleRoomManager.cpp:94-130`): `CreateNewBattleRoom` → `OnCreate`; `RemoveBattleRoom` → `OnDestroy` → `Push` back. `BattleRoomNode::OnDestroy` call `InitData()` yang null `m_pBattleRoom` (`BattleRoomNode.cpp:265`). Associated `Room` managed/reaped `g_RoomNodeManager` → tidak leak dari sisi BattleRoomNode. Tidak double-free observed.
- Risk lifecycle utama: **unbounded `m_vPushStructList` growth** (G-H5) & `m_vMachineStructList`/`m_vUserNode` yang rely explicit erase-on-index (search-based, no OOB) — vektor DoS, bukan leak.

---

## Cross-Cutting Recommendations
1. Centralized state/role gate di gameplay-packet dispatcher (reject spectator, require `MS_PLAY` untuk state-mutating, require owner/host untuk control).
2. Pindah semua match-result computation (damage, contribute, kills, deaths, win/loss, awards) ke authoritative server-side; treat **semua** `CTPK_*_RECORD_INFO`, `CTPK_AWARDING_RESULT`, `CTPK_CURRENT_DAMAGELIST`, `CTPK_*_DIE`, `CTPK_PRISONERMODE` payload sebagai untrusted & validate bounds/sign/sender-entitlement.
3. Bound setiap client-supplied count untuk `reserve()`/loop (`iDamageCnt`, `iMineCnt`, `iStructCnt`, `iCharCnt`, `iAwardSize`, dll) dgn small `MAX_GUARD`.
4. Validate semua enum-typed packet field (`TeamType`, `iSubType`, `iCommand`, `iVoteType`, mode/map index) vs valid range sebelum use.
5. Fix `lstrlen`-first read di `SP2Packet::operator>>(ioHashString&)` & `LPTSTR` operator (`SP2Packet.cpp:619`, `Packet.h:91`) jadi length-bounded.

---

## Prioritas Fix Lokal
1. **G-1..G-7 (P0):** Server-authoritative match result.
2. **G-H3 (P0):** Observer gate di dispatcher.
3. **G-H1/G-H2 (P1):** Owner gate + capacity clamp.
4. **G-H5/G-H6 (P2):** Cap list; validate relay.
5. **G-M7 (P2):** `MAX_GUARD` reserve count.
6. **G-L6 (P1):** Bounded string scan (lintas RC-2).
