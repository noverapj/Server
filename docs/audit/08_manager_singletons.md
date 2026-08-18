# 08 — Manager Subsystem (Singletons)

**Area:** Singleton manager yang di-load di `ioMainProcess::LoadINI`.
**File dibaca:** `ioSaleManager`, `ioQuestManager/Quest`, `ioEventManager`, `ioSuperGashaponMgr`, `ioPetGashaponManager`, `ioPetInfoManager`, `ioRandomBoxManager`, `ioBingoManager`, `ioPirateRouletteManager`, `ioOakBarrelManager`, `TournamentManager`, `HeroRankManager`, `RollBookManager`, `TitleManager`, `GrowthManager`, `FishingManager`, `CompensationMgr`, `MissionManager`, `CostumeManager/CostumeShopGoodsManager`, `SpecialGoodsManager`, `ioPowerUpManager`, `ioCharAwakeManager`, `ioItemRechargeManager`, `ioItemCompoundManager`, `GuildRewardManager`, `TimeCashManager`, `MainProcess.cpp` (LoadINI pattern)

---

## CRITICAL

### S-C1 — Init-failure leak + lifetime leak SEMUA manager singleton (no destructor cleanup)
- **Lokasi:** `MainProcess.cpp:245-248` (`~ioMainProcess`), `:281-594` (LoadINI), `iocpSocketDLL.cpp:30-33` (EndSocket), `Manager.cpp:108`
- **Root cause:** `~ioMainProcess()` hanya call `EndSocket()` (= `WSACleanup`). Tidak `delete` satu pun dari ~60 `m_pXxx` manager pointer yang di-alloc di `LoadINI()` (tidak ada `delete m_pXxx`/`SAFEDELETE( m_p` di mana pun — confirmed grep). Saat `LoadINI()` return `false` partway (mis. `m_pItemPriceMgr->LoadPriceInfo` fail `:384`, `m_pBingoMgr->LoadINIData` `:506`, `m_pPirateRouletteMgr` `:510`, `m_pMissionManager->LoadINI` `:559`, `m_pOakBarrelManager` `:604`, license check `:658`, Xtrap/NProtect/etc `:676-712`), setiap manager sudah `new`-ed di atas titik itu **leak**. `Manager::Prepare` return FALSE & `ioMainProcess::ReleaseInstance()` tidak dipanggil di path failure → singleton + sub-manager leak. Bahkan shutdown normal manager tidak pernah di-free (process exit reclaim, tapi reload/crash path leak).
- **PoC:** Corrupt/truncate late-loaded INI (mis. `config/sp2_oak_barrel.ini`) supaya `m_pOakBarrelManager->LoadINIData` return false `:604`. ~50 manager di-alloc sebelum itu (`m_pItemInfoMgr`, `m_pEventMgr`, `m_pSuperGashaponMgr`, `m_pQuestMgr`, `m_pTournamentManager`, `m_pBingoMgr`, `m_pPirateRouletteMgr`, …) semua leak. Restart gagal berulang → exhaust memory.
- **Fix:** Tambah `DestroyManagers()` yang `SAFEDELETE` setiap `m_pXxx` reverse order, call dari `~ioMainProcess()` & setiap `return false` path di `LoadINI()` (atau RAII `std::unique_ptr` member / cleanup vector).

### S-C2 — Stack buffer overflow di admin-event packet parser
- **Lokasi:** `MainServerNode/MainServerNode.cpp:1072-1085` (`OnAdminEventInsert`), feed `ioEventManager::Update` (`ioEventManager.cpp:1357-1367`) & `EventNode::Update` (`ioEventManager.cpp:108-135`)
- **Root cause:**
  ```cpp
  int iValues[64] = {};
  rkPacket >> iValueCount;
  if(iValueCount < 2) return;
  for(i=0;i<iValueCount;i++) rkPacket >> iValues[i];   // no upper bound vs iValues[64]
  ```
  `iValueCount` dari network packet, hanya cek `< 2`; value 2..INT_MAX accept. `iValueCount > 64` write past `iValues[63]` (stack buffer overflow). Lalu `EventNode::Update` read `iMaxValue = iValues[iIndex++]` & loop `iMaxValue` kali read `iValues[iIndex++]` tanpa cek vs `iValueCount` → attacker-controlled `iMaxValue` read past array juga.
- **PoC:** MainServer compromised/MITM (atau entity yang bisa kirim `STPK`-admin event ke GameServer) kirim event packet `iValueCount = 1000`. Loop write 1000 int ke 64-int stack buffer → stack smash → RCE.
- **Fix:** `if(iValueCount < 2 || iValueCount > 64) return;` & di `EventNode::Update` bound setiap `iValues[iIndex++]` access by `iValueCount`.

---

## HIGH

### S-H1 — ioOakBarrelManager: INI-controlled stack/heap OOB pada `m_dwRewardRandomMax[12]`
- **Lokasi:** `ioOakBarrelManager.cpp:122` (write), `:175` (read), `.h:45` (`DWORD m_dwRewardRandomMax[OAK_BARREL_HOLE]`), `Define.h:840` (`OAK_BARREL_HOLE = 12`)
- **Root cause:** `LoadINIData`: `m_iRewardStepMax = kLoader.LoadInt("reward_max", 0)` (INI-controlled), lalu `for(i=0;i<m_iRewardStepMax;i++) m_dwRewardRandomMax[i] += stReward.m_dwRate;` write fixed 12-element array tanpa cek `i < OAK_BARREL_HOLE`. `reward_max > 12` overrun array (member singleton → heap overrun). Tambahan `GetOneStepReward`/`GetInvalidityRate` punya guard broken `if(iStep < 0 && iStep > GetRewardStepMax()+1)` (pakai `&&` bukan `||`, selalu false) lalu index `m_dwRewardRandomMax[iStep]` dgn `iStep` unbounded.
- **PoC:** Edit `config/sp2_oak_barrel.ini` `[reward rate] reward_max = 100`. Startup loop write 100 entri ke 12-entry array → memory corruption. Runtime caller pass `iStep >= 12` (via broken guard) read OOB.
- **Fix:** `for(i=0; i<m_iRewardStepMax && i<OAK_BARREL_HOLE; i++)` & fix guard `if(iStep < 0 || iStep >= OAK_BARREL_HOLE) return;`.

### S-H2 — ioRandomBoxManager: reload/destructor leak semua gashapon info pointer
- **Lokasi:** `ioRandomBoxManager.cpp:28-42` (destructor), `:55-71` (LoadRandomBoxPackage reload), `:130-169` (LoadCategory)
- **Root cause:** `RandomBoxInfo*`, `RandomBoxCategoryInfo*`, `RandomBoxPackage*`, `sPackageBoxElement*` semua `new`-ed (`:94,130,155,163`) & stored di STL container raw pointer. Reload (`LoadRandomBoxPackage`) & destructor hanya `.clear()` vector/map — **tidak pernah** `delete` pointed-to object. Tiap `CheckNeedReload()`/`LoadINI()` & shutdown leak entire random-box reward tree.
- **PoC:** Trigger repeated INI reload (manager expose `CheckNeedReload`). Tiap reload leak `m_mRandomBoxInfoList` prior, semua category, package, element → unbounded heap growth.
- **Fix:** Iterate & `delete` setiap element pointer sebelum `.clear()`, atau `std::vector<std::unique_ptr<...>>`.

### S-H3 — ioPowerUpManager::ItemPowerDown: OOB read `vPowerUPCode[-1]`
- **Lokasi:** `ioPowerUpManager.cpp:396-406`
- **Root cause:** Guard `if(iItemGrade >= vPowerUPCode.size() || 0 > iItemGrade) return;` — reject negative tapi allow `iItemGrade == 0`. Lalu `iNextItemCode = vPowerUPCode[iItemGrade - 1]` → `vPowerUPCode[-1]` (read sebelum vector buffer). `iItemGrade` derive dari user equipped rare-item code via `ConvertRareItemToRareItemGrade` (return 0 untuk base rare item); player power-downgrade base rare item trigger ini.
- **PoC:** Player dgn base (grade-0) rare extra item kirim power-down request → server read `vPowerUPCode[-1]` → heap OOB read / crash / info leak.
- **Fix:** `if(iItemGrade <= 0 || iItemGrade >= (int)vPowerUPCode.size()) return;`

### S-H4 — Predictable RNG untuk semua gacha/fishing/bingo reward
- **Lokasi:** `MainProcess.cpp:848` (`srand(timeGetTime())`), `Util/IORandom.cpp:86-89` (`Randomize()` = `SetRandomSeed(timeGetTime())`), `ioSuperGashaponMgr.cpp:94/100`, `ioPetGashaponManager.cpp:13-15` (`SetRandomSeed(timeGetTime()+0/1/2)`), `FishingManager.cpp:17-20`, `ioBingoManager.cpp:171/225`, `ioOakBarrelManager.cpp:174`, `ioRandomBoxManager.cpp:253`
- **Root cause:** Global PRNG seeded sekali `timeGetTime()` (millisecond tick, low entropy, attacker-observable/predictable). `IORandom::Randomize()` & per-manager seed juga `timeGetTime()`/`timeGetTime()+n`. Gacha result (`ioSuperGashaponMgr::SendSuperGashaponRandPackage`, `ioRandomBoxManager::SendRandomBoxRandPackage`), pet gacha, fishing success/item, bingo reward shuffle, oak-barrel reward derive dari seed ini atau `rand()`. Client yang estimate `timeGetTime()` server (mis. measure latency + tahu startup time) bisa predict gacha outcome.
- **PoC:** Attacker sync ke server tick, kirim gacha-use packet di tick terpilih untuk land rare-package bracket; reward table bounded tapi *selected* prize jadi deterministic.
- **Fix:** Seed dgn CSPRNG (`BCryptGenRandom`/`rand_s`), jangan reuse `timeGetTime()`, pakai per-request PRNG seeded dari CSPRNG bukan global long-lived stream.

---

## MEDIUM

### S-M1 — MissionManager: leak saat `Mission::Create` fail & duplicate-code insert
- **Lokasi:** `MissionManager.cpp:366-371` & `:817-825`
- **Root cause:** `Mission* pMission = new Mission; if(!pMission) continue; if(!pMission->Create(...)) return FALSE;` — saat `Create` fail, freshly `new`-ed `pMission` tidak di-delete & `LoadINI` return FALSE (init failure). Juga `m_mXxxMissionTable.insert(make_pair(code, pMission))` silent fail di duplicate `code` (map), leak orphan `pMission`.
- **PoC:** Duplicate `missionX_code` di `sp2_daily_mission.ini`, atau malformed mission `values` string → `Create` fail → leak + init abort.
- **Fix:** `delete pMission;` saat `Create` fail / sebelum duplicate-insert; pakai `insert` return-value check.

### S-M2 — TitleManager: leak saat duplicate code & OOB saat premium table empty
- **Lokasi:** `TitleManager.cpp:76-82` (leak dup `dwCode`), `:161` & `:222` (`m_vPremiumTable[0]`/`m_vPremiumTable[i]` no empty check)
- **Root cause:** `new TitleData; ... m_mTitleTable.insert(make_pair(dwCode, pData))` — jika INI duplicate `code`, `insert` fail & `pData` leak. `IsLevelUp`/`GetMaxPremiumLevel` access `m_vPremiumTable[0]` unconditional; jika `sp2_title.ini` tidak ada `[premium1]` section → vector empty → OOB read → crash saat title level-up check.
- **Fix:** Check `insert().second`; guard `if(m_vPremiumTable.empty()) return FALSE;`.

### S-M3 — ioPetGashaponManager: INI-controlled OOB pada `m_vRandomTotalInRank`/`m_vGradedTag`
- **Lokasi:** `ioPetGashaponManager.cpp:116/142/168/194/220` (`m_vRandomTotalInRank[RANKx-1]`), `:326` (`m_vGradedTag[iRank-1].iEndIndex`), `:328` (`m_vAllRankPetRandomInfo[i]`)
- **Root cause:** `m_vRandomTotalInRank.reserve(iRankCount)` lalu index dgn fixed `[RANKD-1 .. RANKS-1]` (0..4). `reserve` tidak set size; hanya `iRankCount` element `push_back`-ed. Jika INI `max_rank_count` 1..4, index itu OOB (UB). `GetPetCodeWithRandom` juga pakai `m_vGradedTag[iRank-1].iEndIndex` (hanya 5 grade tag) & loop `i <= iEndIndex` vs `m_vAllRankPetRandomInfo` tanpa size check; inconsistent per-rank `max_pet_count` INI bisa drive read past vector.
- **PoC:** Set `max_rank_count = 2` di `sp2_pet_gashapon.ini` → write `m_vRandomTotalInRank[2..4]` OOB.
- **Fix:** Validate `iRankCount == 5` sebelum index fixed rank slot; bound `GetPetCodeWithRandom` loop by `m_vAllRankPetRandomInfo.size()`.

### S-M4 — ioQuest: duplicate/out-of-range reward-selection index grant duplicate reward
- **Lokasi:** `ioQuest.cpp:1236-1282` (`SetQuestReward`), `:1301-1318` (`PackQuestReward`), `QuestVariety.cpp:59-64` (`GetRewardPresentIndex`)
- **Root cause:** Quest select-style, client supply `vSelIndexes`. Jika `vSelIndexes.size() == GetRewardSelectNum()` server grant `GetRewardPresentIndex(vSelIndexes[i])` tiap. Tidak cek index distinct atau di `[0, GetMaxRewardPresent())`. Client kirim duplikat (mis. `{2,2}`) terima selectable reward sama dua kali. `GetRewardPresentIndex` return 0 saat OOB, & jika INI reward punya `RewardIndex = 0` (`ioQuestManager.cpp:370`), OOB index grant reward itu.
- **PoC:** Client complete 2-reward-select quest & kirim `vSelIndexes = {2,2}` → terima reward #2 dua kali.
- **Fix:** Validate `vSelIndexes` entry di `[0, GetMaxRewardPresent())` & unique; reject `RewardIndex == 0` di INI.

### S-M5 — ioEventManager::ioPlazaMonsterEventNode::LoadINI: `atoi(NULL)` crash saat empty day string
- **Lokasi:** `ioEventManager.cpp:266-275`
- **Root cause:** `a_rkLoader.LoadString(szKey, "", szBuf, MAX_PATH); char* pos = strtok_s(szBuf, ".", &next); int day = atoi(pos);` — jika INI key `day_%d` missing/empty, `szBuf` = `""`, `strtok_s` return `NULL`, & `atoi(NULL)` UB (access violation). Reachable saat server startup / event reload.
- **PoC:** Tambah `[monster]` event block dgn `total_day_1 = 2` tapi missing/empty `day_1` → server crash saat startup atau event INI reload.
- **Fix:** `if(pos == NULL) { LOG(...); continue; }`

### S-M6 — SpecialGoodsManager::SetSpecialGoodsList: unbounded packet-driven vector growth (DoS)
- **Lokasi:** `SpecialGoodsManager.cpp:90-108`
- **Root cause:** `PACKET_GUARD_VOID_READ(kPacket, iSize); for(i=0;i<iSize;i++){ ...; m_vSpecialGoodsList.push_back(stGoods); }` — `iSize` dari network packet no upper bound. Huge `iSize` → unbounded alloc / CPU loop.
- **PoC:** MainServer (atau attacker inject channel itu) kirim `iSize = 2,000,000,000` → server thrash/OOM.
- **Fix:** Cap `iSize` ke sane max (mis. 1024) sebelum loop.

### S-M7 — Garena quest event array OOB
- **Lokasi:** `MainProcess.cpp:353-363` (write loop), `MainProcess.h:115` (`int m_iEventMainQuestIDXArr[50]`), `MainProcess.h:286` (`GetEventMainIDX(int i)` no bounds)
- **Root cause:** `m_iEventMaxMainCount = LoadInt("MAXMainCount", 1);` lalu `for(i=0;i<m_iEventMaxMainCount;i++) m_iEventMainQuestIDXArr[i] = ...` tanpa cek vs fixed size 50. INI `MAXMainCount > 50` overrun array. `GetEventMainIDX(i)` juga no bounds check.
- **PoC:** `ls_config_game.ini` `[Garena_Event] MAXMainCount = 100` → heap overrun singleton ioMainProcess saat startup.
- **Fix:** `for(i=0; i<min(m_iEventMaxMainCount,50); i++)` & bounds-check `GetEventMainIDX`.

### S-M8 — ioPowerUpManager: reload grow `m_vRareItemNeedMaterialCnt` tanpa clearing
- **Lokasi:** `ioPowerUpManager.cpp:60-196` (`LoadINI` tidak call `Init()`), `:153` (`push_back(0)` tiap load), `:162/172`
- **Root cause:** `CheckNeedReload` call `LoadINI`, tapi `LoadINI` tidak call `Init()` clear map/vector. `m_vRareItemNeedMaterialCnt.push_back(0)` tiap load, jadi tiap reload append fresh `[0, grade1..]` block, shift index yang dipakai `ItemPowerUp/Down` (`m_vRareItemNeedMaterialCnt[iItemGrade+1]`) → wrong material cost / OOB saat vector grow.
- **Fix:** Call `Init()` di top `LoadINI`.

### S-M9 — ioBingoManager: `LoadINIData` tidak clear vector sebelum load + pakai `rand()`
- **Lokasi:** `ioBingoManager.cpp:47-153` (no `Init()` sebelum load), `:171` (`rand() % (*iter).percent + 1`), `:225` (`IORandom` seed `timeGetTime()`)
- **Root cause:** `LoadINIData` hanya run `Init()` dari constructor, bukan start reload, jadi re-call append duplicate reward ke `m_vecReward`/`m_vecAllBingo`/`m_DummyInfoVec`. Reward selection pakai `rand()` (predictable global) & `IORandom` seed `timeGetTime()`.
- **Fix:** Call `Init()` di start `LoadINIData`; pakai CSPRNG.

---

## LOW

### S-L1 — ioPetGashaponManager: dead-code infinite loop/OOB di single-arg `GetRankWithRandom`
- **Lokasi:** `ioPetGashaponManager.cpp:262-279` (`for(int i=0; iRankrandomSize; i++)` — condition constant size, bukan `i < size`)
- Latent; single-arg overload tidak pernah dipanggil (confirmed hanya 3-arg overload `:309` invoked).
- **Fix:** `for(int i=0; i < iRankrandomSize; i++)`. Hapus dead overload.

### S-L2 — ioSuperGashaponMgr: `erase` dari wrong container di sub-package sold-out path
- **Lokasi:** `ioSuperGashaponMgr.cpp:389-405`
- **Root cause:** Di `SendSuperGashaponRandSubPackage`, `iter` iterasi `m_vSuperGashaponSubPackageList`, tapi sold-out branch call `pInfo->m_vSuperGashaponPackageList.erase(iter)` (*non-sub* list) — UB (erase iterator beda container). Corrupt main package list.
- **Fix:** Erase dari `m_vSuperGashaponSubPackageList`.

### S-L3 — ioRandomBoxManager: dead `IORandom Rand; Rand.SetRandomSeed(timeGetTime())` tidak dipakai
- **Lokasi:** `ioRandomBoxManager.cpp:252-253`
- Dead code; seeded `Rand` unused — reward decide earlier oleh `pkInfo->m_RandomBoxRandom`.
- **Fix:** Hapus.

### S-L4 — ioCharAwakeManager::IsLoadedAwakeDay logic bug
- **Lokasi:** `ioCharAwakeManager.cpp:35-49`
- **Root cause:** Return `false` jika ada existing entry beda `iAwakeDay`; test "all loaded days equal this day" bukan "this day in list". Cause `m_vAwakeAddDateInfo` miss distinct day saat load.
- **Fix:** Return `true` hanya saat matching entry found (atau `std::find`).

### S-L5 — CostumeShopGoodsManager: inner loop variable shadow outer `i`
- **Lokasi:** `CostumeShopGoodsManager.cpp:155-188` (`for(int i=0;i<iMachineCount;i++)` & inner `for(int i=0;i<100;i++)`)
- OK secara scoping, tapi error-prone.
- **Fix:** Rename inner index.

### S-L6 — CompensationMgr: `Destroy()` clear nothing; possible re-claim dgn stale logout time (SUSPECTED)
- **Lokasi:** `CompensationMgr.cpp:25-27` (empty `Destroy`), `:77-100` (`SendCompensation`)
- **Root cause:** `m_vCompensationInfoVec` tidak pernah di-clear (hanya grow di `RegistCompensation`). `SendCompensation` gate eligibility `cUserLogOutTime > cStartDate`; user dgn zero/stale logout time bisa re-receive compensation sama di multiple login dalam window.
- **Fix:** Clear/age-out vector; track per-user claim state.

### S-L7 — ioSaleManager `m_pSaleMgr`: dead allocation, tidak dipakai/freed
- **Lokasi:** `MainProcess.cpp:381` (`m_pSaleMgr = new ioSaleManager;` — no `LoadINI()` called, tidak reference lagi, tidak delete), vs `g_SaleMgr` Singleton dipakai everywhere
- Member instance allocated tapi tidak init/dipakai (real usage = `Singleton<>` `g_SaleMgr`). Leak (covered S-C1) & pointless. Pattern dual-instance risk ada untuk manager lain yang sama-sama `new` ke `m_pXxx` & access via `g_XxxMgr`.
- **Fix:** Hapus member allocation, atau konsisten pakai single instance & delete di `DestroyManagers`.

### S-L8 — INI-controlled allocation size (DoS via config)
- **Lokasi:** `GrowthManager.cpp:77-85`, `RollBookManager.cpp:67/82`, `GuildRewardManager.cpp:52/88`, `ioPetInfoManager.cpp:59/78`, `ioCharAwakeManager.cpp:60-95`, `ioSuperGashaponMgr.cpp:74`, `TitleManager.cpp:61/84`, `TimeCashManager.cpp:45`
- **Root cause:** Banyak `LoadInt("max_xxx", 0)` count drive `reserve`/`push_back` loop no upper bound. Config malicious `max_xxx = 2,000,000,000` → huge alloc/long loop startup. Tidak remotely exploitable kecuali config writable attacker.
- **Fix:** Clamp semua INI count ke reasonable max.

### S-L9 — Inconsistent/dead `if(!pXxx)` NULL check setelah `new`
- **Lokasi:** `MainProcess.cpp:451/455/474/478/482/486/497/501/516/519/552/566/572/581/585/589/593/577`; `TitleManager.cpp:77`; `MissionManager.cpp:367/818`; `RollBookManager.cpp` (`if(!m_pRollBookManager) return false` `MainProcess.cpp:566` dead karena `new` throw)
- **Root cause:** Standard `new` throw `std::bad_alloc` saat fail; tidak return NULL, jadi `if(!m_pXxx) return false` dead code. Inconsistency (beberapa check, kebanyakan tidak) false sense safety. Jika build `/EHsc` & alloc fail → exception propagate through `LoadINI` & unwind tanpa free manager sudah-allocated (sama leak S-C1, via path beda).
- **Fix:** Konsisten `new(std::nothrow)` + cleanup on fail, atau hapus dead check & rely RAII.

### S-L10 — ioEventManager::LoadINI duplicate-event handling rely commented-out dedup
- **Lokasi:** `ioEventManager.cpp:1285-1289` (dedup `IsExist` check commented out)
- **Root cause:** Comment bilang same-type event intentionally allowed, tapi reload path (`bCreateLoad=false`) call `GetNode` yang return hanya *first* matching node, jadi entry same-type tambahan dari INI saat reload silently ignored. Latent config-state inconsistency.
- **Fix:** Document/resolve intended duplicate-type semantics.

---

## Prioritas Fix Lokal
1. **S-C1 (P2):** `DestroyManagers()` + RAII.
2. **S-C2 (P0):** `if(iValueCount > 64) return;` admin event.
3. **S-H1/S-H3 (P1):** Bounds check oak-barrel & power-up.
4. **S-H2 (P2):** Delete-on-reload RandomBox.
5. **S-H4 (P2):** CSPRNG.
6. **S-M1..S-M9 (P2):** Init-on-reload, distinct validation, cap count, atoi(NULL) guard.
