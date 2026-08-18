# 06 — Item / Inventory / Trade / Ekonomi

**Area:** Logic ekonomi paling critical (dupe, integer, race, item loss).
**File dibaca:** `ioInventory.cpp/.h`, `Item.cpp/.h`, `ioItemInfoManager`, `ioSetItemInfo*`, `ioEtcItem*`, `ioExtraItemInfoManager`, `ioUserEtcItem`, `ioUserExtraItem`, `ioUserCoin`, `ioUserPresent`, `ioUserCostume`, `ioUserMedalItem`, `ioUserAccessory`, `ioEquipSlot`, `TradeInfoManager`, `TradeSyncManger`, `ioSaleManager`, `ioPresentHelper`, `ioItemCompoundManager`, `ioItemRechargeManager`, `User.cpp` (sell/trade handlers)

---

## E-1 — HIGH: ETC-item USE count decrement in-memory only, tidak persist (DUPE non-atomic)
- **Lokasi:** `User.cpp:17980` (`OnEtcItemUse`), `ioEtcItem.cpp:200` (`ioEtcItem::OnUse`), `:1872` (`ioEtcItemGashapon::_OnUse`)
- **Tipe:** dupe / race (non-atomic consume-vs-effect)
- **Root cause:** `OnEtcItemUse` call `pEtcItem->OnUse(...)` lalu return. **Tidak** call `SaveEtcItem()`/`SaveUserData()`. `OnUse`/`_OnUse` hanya in-memory `AddUse(-1)` + `SetEtcItem`/`DeleteEtcItem` (mark `m_bChange=true`). Persistensi hanya di periodic `SaveData()` / logout. Sementara reward gashapon queued ke main server (`AddPresentMemory` → `SendPresentMemory`, `ioPresentHelper.cpp:1658/1760`) = DB write terpisah.
- **PoC:** Kirim `CTPK_ETCITEM_USE` untuk gashapon/coin etc item. Reward present dispatched ke main server (persisted di sana). Langsung drop TCP connection (atau crash link) sebelum game-server periodic `SaveEtcItem` run. Login berikutnya etc-item count reload dari DB di value pre-use, tapi gashapon reward present sudah delivered → free roll. Repeat.
- **Fix:** Call `SaveEtcItem()` + `SaveUserData()` di `OnEtcItemUse` (atau `ioEtcItem::OnUse`) untuk setiap `_OnUse` yang mutate `m_iValue1`; persist decrement & reward dalam 1 DB transaction (atau persist decrement sebelum dispatch reward).

### E-2 — HIGH: Accessory COMPOSE material slot tidak cek distinct (ECONOMY EXPLOIT)
- **Lokasi:** `User.cpp:35911` (`OnAccessoryCompose`), `ioUserAccessory.cpp:448` (`OnCompose`)
- **Root cause:** `OnCompose(iAccessoryIndex, iMaterialIndex1, iMaterialIndex2, iMaterialIndex3)` delete 3 material (`DeleteAccessoryItem` `:453-455`) lalu unconditional apply compose bonus (`SetComposeCode/SetComposeValue` + DB update `:472-478`). `DeleteAccessoryItem` (`:146-161`) erase dari map + fire DB delete; jika index sama 3x, call pertama delete, 2 berikutnya no-op (key gone) — tapi compose tetap fully applied. Catalyst etc item (`EIT_ETC_ACCESSORY_COMPOSE`) consume 1x via `pEtcItem->OnUse` (`35959`).
- **PoC:** Punya ≥1 accessory A. Kirim `CTPK_ACCESSORY_COMPOSE` dgn `iBaseIndex = B`, `iMaterialIndex1 = iMaterialIndex2 = iMaterialIndex3 = index(A)`. Result: A consume 1x, compose bonus fully applied ke B (DB-persisted `:477`). Net: spend 1 catalyst + 1 accessory instead of 1 catalyst + 3 accessories. Repeat compose banyak accessory di 1/3 material cost.
- **Fix:** Validate `iMaterial1/2/3` pairwise distinct & distinct dari `iAccessoryIndex`; abort jika tidak. Apply compose hanya setelah 3 delete sukses.

### E-3 — HIGH: `User::AddMoney` wipe entire peso balance saat single credit > 10M (MONEY WIPE)
- **Lokasi:** `User.cpp:4549` (`AddMoney`)
- **Root cause:** `:4567-4572`: `if((i64Money < 0) || (i64Money > 10000000))` → `m_user_data.m_money = 0; CloseConnection();`. Setiap credit legit >10M diperlaku sebagai cheat & **zero balance**. Banyak flow legit credit >10M satu shot: trade-sale proceeds via `PRESENT_PESO` (`ioUserPresent.cpp:406`), `PresentSell` soldier/costume/accessory/medal (`ioUserPresent.cpp:922/943/983…`), `OnCostumeSell`/`OnAccessorySell`/`OnMedalItemSell`/`OnDecoSell` (`User.cpp:35583/35888/27669/28666`), `ioEtcItem::OnSell` (`ioEtcItem.cpp:346`). User jual item >10M (marketplace explicitly allow — seller set `iItemPrice` di `OnTradeCreate`) terima `PRESENT_PESO`; receiving call `AddMoney(>10M)` → balance 0 + disconnect. Present juga di-delete (`DeletePresentData`, `:411`) → proceeds hilang DAN pre-existing money hilang.
- **PoC:** Register trade item price 12,000,000 (`OnTradeCreate`, `User.cpp:29468` `iItemPrice` client-controlled uncapped). Accomplice beli. Seller terima `PRESENT_PESO` value 12,000,000. Seller call `CTPK_PRESENT_RECV` → `PresentRecv` → `AddMoney(12000000)` → `m_user_data.m_money = 0`, connection closed. Seller kehilangan semua money + sale.
- **Fix:** Hapus absolute 10M cutoff dari `AddMoney` (atau raise ke `INT64_MAX`/cap-balance) lalu guard **hasil** overflow/negatif, bukan input. Keep anti-cheat di *delta* via server-authoritative source, bukan gameplay credit legit.

### E-4 — HIGH: Trade-item REGISTER delete item sebelum main server konfirmasi listing (ITEM LOSS)
- **Lokasi:** `User.cpp:29462` (`OnTradeCreate`)
- **Root cause:** Order: `RemoveMoney(tax)` (`:29517`) → `m_UserExtraItem.DeleteExtraItem(iExtraValue)` (`:29529`) → `m_UserExtraItem.SaveData()` (`:29530`, persist deletion) → `g_DBClient.OnSelectCreateTrade(...)` (`:29538`, ask main server create listing). Item permanently gone dari user sebelum listing exist di main server. Jika main-server message lost/reject/user disconnect antara → item destroyed tanpa listing & tanpa proceeds. Tidak ada rollback.
- **PoC:** Kirim `CTPK_TRADE_CREATE` lalu langsung sever game↔main link (atau flood main server). Extra item dihapus & saved; listing tidak pernah created. Item lost.
- **Fix:** Create listing di main server dulu; delete item dari seller setelah main server ack `TRADE_CREATE` success, atau keduanya dalam 1 DB transaction di main server.

### E-5 — MEDIUM-HIGH: Trade BUY price math pakai float conversion `__int64` (INTEGER/OVERFLOW)
- **Lokasi:** `MainServerNode/MainServerNode.cpp:1540` (`OnTradeItemComplete`)
- **Root cause:** `iResultPeso = iItemPrice + (iItemPrice * g_TradeInfoMgr.GetBuyTexRate());` — `iItemPrice` = `__int64`, dikali `float` tax rate, dijumlah `__int64 + float`, assign ke `__int64`. Untuk `iItemPrice` besar (seller set, uncapped di `OnTradeCreate`), float conversion loss precision & bisa overflow negatif; `if(iCurPeso < iResultPeso)` (`:1544`) compare vs garbage/negatif. `iResultPeso` negatif lewat check (money ≥ negatif), `RemoveMoney(iResultPeso)` trigger guard `<0` (`User.cpp:4577`) → money zeroed + disconnect (buyer-side grief). Pattern float-mul sama di `OnTradeCreate` (`:29504/29506`) untuk registration tax.
- **PoC:** Seller list item `iItemPrice` near `INT64_MAX`. Accomplice beli. `iResultPeso` overflow/float-truncate ke wrong value; bergantung sign, buyer di-charge wrong (tiny) atau `RemoveMoney` zero money buyer.
- **Fix:** Clamp `iItemPrice` ke sane 64-bit max di `OnTradeCreate` (mis. ≤ 1,000,000,000). Compute tax integer math (`iItemPrice + iItemPrice * rate_int / 10000`); check `iResultPeso > 0` & `< money` sebelum `RemoveMoney`.

### E-6 — HIGH: Item COMPOUND (2-item) tidak tolak target==victim slot (ITEM DESTRUCTION)
- **Lokasi:** `ioItemCompoundManager.cpp:426` (`CheckCompoundSuccess`)
- **Root cause:** Tidak cek `iTargetSlot != iVictimSlot`. Jika sama: `GetExtraItem(iTargetSlot)` & `GetExtraItem(iVictimSlot)` return item sama; `DeleteExtraItem(iVictimSlot)` (`:496`) delete; `SetExtraItem(kTargetSlot)` (`:507/513/521`) no-op karena slot blanked (`m_iIndex==0`); `SaveExtraItem()` (`:557`) **persist deletion** & function return 0 (success). Item permanent destroyed, reinforce change hilang.
- **PoC:** Kirim `CTPK_ETCITEM_USE` dgn compound etc item, payload `iTargetSlot == iVictimSlot` (`User.cpp:27723`). Item di slot itu dihapus, tidak ada reinforce, saved ke DB.
- **Fix:** Reject `iTargetSlot == iVictimSlot` di top `CheckCompoundSuccess`.

### E-7 — MEDIUM: Multiple-Compound: no distinctness check 3 input slot; deletion-before-creation no rollback
- **Lokasi:** `ioItemCompoundManager.cpp:1017` (`CheckMultipleCompound`)
- **Root cause:** `iItem1/iItem2/iItem3` tidak cek pairwise distinct (`:1039-1067`). Jika 2/3 sama, `DeleteExtraItem` pertama consume item, lain no-op; function return `ITEM_MULTIPLE_COMPOUND_DEL_ERROR` (`:1153`) **tanpa restore item sudah di-delete**. Karena `SaveExtraItem()` hanya di path success (`:1245`), in-memory deletion tidak persist & item kembali saat relog — tapi selama session client lihat item gone (state inconsistency). Semua 3 deletion (`:1135-1151`) sebelum result item generate; jika `GetRandomPeriod` return `<0` (`:1188`) atau `AddExtraItem` return `<=0` (`:1205`) → function return false tanpa recreate consumed item (in-memory only, recoverable relog).
- **PoC:** Kirim `CTPK_ETCITEM_USE` multiple-compound etc item `iItem1 == iItem2 == iItem3`. Satu item consumed in-memory; client lihat error & item hilang sampai relog.
- **Fix:** Require pairwise-distinct slot; delete 3 source hanya setelah result item sukses generate, atau rollback deletion di failure.

### E-8 — HIGH: Accessory REINFORCE tidak tolak base==material (ITEM LOSS)
- **Lokasi:** `ioUserAccessory.cpp:488` (`OnReinforce`)
- **Root cause:** Tidak cek `iAccessoryIndex != iMaterialIndex`. Jika sama, `pAccessory` & `pMaterial` alias object sama; accessory di-reinforce (`:534`), DB-update (`:535`), lalu `DeleteAccessoryItem(iMaterialIndex)` (`:542`) delete accessory sama (baru di-reinforce). Permanent loss base accessory.
- **PoC:** Kirim `CTPK_ACCESSORY_REINFORCE` dgn `iBaseIndex == iMaterialIndex`.
- **Fix:** Reject equal index.

### E-9 — MEDIUM (SUSPECTED): `PresentSell` (dan semua sell flow) credit money sebelum item durably deleted (DUPE on DB-delete fail)
- **Lokasi:** `ioUserPresent.cpp:922` (PRESENT_SOLDIER), `:943` (PRESENT_DECORATION), `:983/1005/1044/1065/1086/1107/1179` (PRESENT_ETC_ITEM/etc.)
- **Root cause:** Pattern: `AddMoney(iResellPeso)` → `SaveUserData()` (persist money) → `DeletePresentData(...)` (erase in memory + fire-and-forget `g_DBClient.OnDeletePresent`). `DeletePresentData` (`:66-103`) kirim `OnUpdatePresentData`+`OnDeletePresent` lalu return. Jika DB delete gagal setelah `SaveUserData` sukses, saat relog present reappear (DB masih punya) sementara money sudah banked → sell lagi → dupe. Re-sell sesi sama blocked in-memory (vector entry erased) → window butuh relog + dropped DB delete.
- **PoC:** Sell valuable present; induce DB-delete failure (DB stall/timeout) tepat setelah `SaveUserData` commit; relog; present kembali & money kept.
- **Fix:** Delete present (persist deletion) **sebelum** credit money, atau 1 stored procedure atomic delete present row + credit peso.

### E-10 — MEDIUM: Sell flow Costume/Accessory/Medal/Deco non-atomic (item-delete fire-forget + separate money save)
- **Lokasi:** `User.cpp:35543` (`OnCostumeSell`), `:35824` (`OnAccessorySell`), `:27614` (`OnMedalItemSell`), `:28616` (`OnDecoSell`)
- **Root cause:** Tiap delete item via fire-and-forget DB call & credit money via `SaveUserData`/`SaveInventory`/`SaveMedalItem` terpisah. Partial DB failure antara → item loss atau money dupe. `OnDecoSell` tambahan **tidak** call `SaveInventory`/`SaveUserData` di handler (hanya `RemoveSlotItem`+`AddMoney` in-memory; rely periodic `SaveData`).
- **PoC:** Sell saat DB connection partial failing → diverge.
- **Fix:** Wrap item-deletion + money-credit di 1 DB transaction.

### E-11 — MEDIUM: `ioEtcItem::OnSell`/`PresentSell` integer overflow `GetSellPeso() * count`
- **Lokasi:** `ioEtcItem.cpp:329` (`int iResellPeso = GetSellPeso() * iValue1;`), `ioUserPresent.cpp:968` (`pEtcItem->GetSellPeso() * rkData.m_iPresentValue2`)
- **Root cause:** `int * int` no 64-bit promotion. `GetSellPeso()` & `iValue1`/`m_iPresentValue2` server-side int tapi `iValue1` bisa sampai `MAX_COUNT` (99999/999999, `ioEtcItem.h:712-715`). Guard tunggal `if(iResellPeso < 0)` (`ioEtcItem.cpp:330`) hanya catch sign-negative overflow; wrap yang land positive grant wrong (biasanya lebih kecil, kadang arbitrary) amount & item tetap di-delete.
- **PoC:** Konfigurasi/punya etc item dgn `GetSellPeso` besar & max count sehingga product wrap positive; sell; terima wrapped amount.
- **Fix:** Compute `__int64`, clamp ke `INT32_MAX`, validate `>0` sebelum `AddMoney`.

### E-12 — MEDIUM: `OnEtcItemBuy` (peso) deduct money sebelum grant item, no rollback saat `AddEtcItem` fail
- **Lokasi:** `User.cpp:17771` (`RemoveMoney(iPeso)`) & `:17882` (`AddEtcItem`), failure return `:17888`
- **Root cause:** `RemoveMoney` dipanggil (in-memory) jauh sebelum `m_UserEtcItem.AddEtcItem`. Jika `AddEtcItem` fail (return false, `:17882-17889`) handler return `ETCITEM_BUY_EXCEPTION` **tanpa** restore deducted peso. Karena `SaveUserData` tidak reached di path itu, relog restore money (DB tidak update) — recoverable, tapi selama session user lihat money gone & no item.
- **PoC:** Isi etc-item table ke cap, lalu `CTPK_ETCITEM_BUY` peso etc item; `AddEtcItem` fail; money gone in-memory untuk session.
- **Fix:** Grant item dulu (atau pre-reserve slot), lalu deduct money; atau refund `AddMoney(iPeso)` di failure branch.

### E-13 — MEDIUM: `TradeSyncManager::SendTradeItemList` pakai `static` buffer shared (RACE)
- **Lokasi:** `TradeSyncManger.cpp:80` (`static vTradeSyncInfo vSendTradeItem;`)
- **Root cause:** Send buffer `static` (function-local static) & `clear()`ed per call. Jika 2 user `SendTradeItemList` overlap (multi-threaded user-node dispatch), 1 call `clear()`/`push_back` interleave dgn `pUser->SendMessage` loop → corrupt trade list dikirim ke client.
- **Fix:** `vSendTradeItem` local (stack).

### E-14 — MEDIUM: `TradeSyncManager::RecvAllTradeItem`/`RecvAddTradeItem` abort whole batch di duplikat pertama
- **Lokasi:** `TradeSyncManger.cpp:62` & `:205`
- **Root cause:** Saat duplikat `m_dwTradeIndex`, code `return;` (bukan `continue;`) → 1 duplikat drop sisa batch sync, desync game-server trade-item cache dari main server (missing listing → buyer lihat stale data, possible purchase already-sold → main server reject tapi game state inconsistent).
- **Fix:** `continue` untuk duplikat.

### E-15 — LOW: `ioEtcItemRecharge::_OnUse` decrement catalyst sebelum validate target
- **Lokasi:** `ioEtcItem.cpp:6516` (juga `:6605` accessory recharge)
- **Root cause:** `rkSlot.AddUse(-1)` + `SetEtcItem/DeleteEtcItem` (`:6519-6527`) sebelum `SetRechargeExtraItem` validate `iSlotIndex/iItemCode` (`:6534`). Target bad → catalyst consumed in memory tapi recharge fail; `OnUse` return false & caller tidak save → relog restore catalyst (transient).
- **Fix:** Validate target (dan `m_iItemCode`) sebelum decrement catalyst.

### E-16 — LOW: `ioUserExtraItem::m_iCurMaxIndex` unbounded `int` increment tiap add
- **Lokasi:** `ioUserExtraItem.cpp:294,311`
- **Root cause:** `m_iCurMaxIndex++` tanpa wrap guard. Setelah ~2³¹ item added selama server lifetime → wrap ke 0/negatif, collide dgn sentinel "blank slot" (`m_iIndex==0` check `:292`) dan/atau duplikat `m_iIndex` → `GetExtraItem`/`SetExtraItem`/`DeleteExtraItem` (match by `m_iIndex`) operasi match pertama — mungkin item salah.
- **Fix:** 64-bit index atau guard wrap; jangan pakai 0 sebagai live index.

### E-17 — LOW: `ioInventory::AddSlotItem` create `NEW_INDEX` row via fire-and-forget insert; `SaveData` skip `NEW_INDEX`
- **Lokasi:** `ioInventory.cpp:413-418` (insert), `:255-259` (save skip NEW_INDEX)
- **Root cause:** Saat no empty slot, new row `NEW_INDEX` di-push & `InsertDBInventory` fire. `SaveData` deliberately tidak update `NEW_INDEX` row. Jika insert DB fail, in-memory row exists dgn `NEW_INDEX` tapi DB tidak → saat relog deco gone. `DBtoNewIndex` assign real index 1 per 1 via `OnSelectInvenIndex` serialize di DB response.
- **Fix:** Treat insert failure explicit (retry/rollback in-memory row) daripada rely `DBtoNewIndex` callback.

### E-18 — LOW: `OnUseItem` (`CTPK_USE_ITEM`) trust client `eState` & broadcast
- **Lokasi:** `User.cpp:12582`
- **Root cause:** `eState` read dari packet (`:12596`) & written verbatim ke broadcast `STPK_OBJECT_ITEM_RELEASE` (`:12611`) tanpa validation. Client forge arbitrary `eState` ditampilkan ke client lain. Tidak ekonomi impact, tapi unvalidated client-controlled field di broadcast.
- **Fix:** Validate `eState` vs enum atau compute server-side.

### E-19 — MEDIUM: `OnTradeCreate` accept client-controlled `iItemPrice` no server bounds
- **Lokasi:** `User.cpp:29468`
- **Root cause:** Seller listing price (`iItemPrice`, `__int64`) read dari packet & forwarded ke main server hanya dgn implicit `RemoveMoney` negative-check guard. Price 0 avoid registration tax entirely; price absurdly large interact buruk dgn float tax math (E-5) & 10M `AddMoney` cap (E-3) saat sale complete.
- **Fix:** Clamp `iItemPrice` ke `[1, MAX_SELL_PRICE]` server-side sebelum tax/forward logic.

### E-20 — LOW: `AddMoney` no overflow guard di *sum*
- **Lokasi:** `User.cpp:4553` (`m_user_data.m_money += i64Money;`)
- **Root cause:** Hanya input `i64Money` range-check (≤10M), bukan hasil sum. Jika `m_user_data.m_money` sudah near `INT64_MAX` (impractical given 10M-per-call cap) → sum overflow. Low likelihood tapi guard incomplete.
- **Fix:** Check `m_user_data.m_money > INT64_MAX - i64Money` sebelum add.

---

## Memory Leak Assessment
- `ioInventory::~ioInventory` (`cpp:23`) `clear()` `vector<ITEMDB>` (value type) — OK.
- `ioEquipSlot::ClearSlot` (`ioEquipSlot.cpp:22`) `SAFEDELETE` tiap `m_EquipSlot[i]` — OK; `EquipItem` out-of-range slot `SAFEDELETE` passed item (`cpp:41`) — OK (no leak, walau silently delete caller item).
- `ioUserEtcItem`/`ioUserExtraItem`/`ioUserCostume`/`ioUserAccessory`/`ioUserMedalItem`/`ioUserPresent` destructor `clear()`/`erase` container; `Accessory`/`Costume` value type di map. Tidak raw `new` tanpa delete di sell/trade path.
- Tidak ada confirmed memory leak di item/inventory/trade path. Hazard tunggal: `ioEquipSlot::EquipItem` delete out-of-range `pItem` (`cpp:41`) = double-free risk jika caller masih reference — minor.

---

## Prioritas Fix Lokal
1. **E-1/E-2/E-3/E-4/E-6/E-8 (P1):** Atomicity + distinct validation + hapus 10M wipe + clamp price.
2. **E-5/E-11/E-19 (P1):** Integer 64-bit math, clamp price, no float.
3. **E-9/E-10/E-12 (P2):** Delete-before-credit, refund on fail.
4. **E-13/E-14 (P2):** Static buffer race, continue not return.
