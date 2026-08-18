# 10 — Inter-Server (MainServer/BillingRelay/SS)

**Area:** Komunikasi antar-server: MainServer, BillingRelay, game-to-game SS, DBAgent.
**File dibaca:** `MainServerNode/*`, `BillingRelayServer/*`, `NodeInfo/AcceptorServerNode.cpp/.h`, `NodeInfo/ServerNode.cpp/.h`, `NodeInfo/LicenseManager.cpp/.h`, grep `SSTPK_*`/`STPK_*`/`MTPK_*`/`BRPK_*`

---

## Foundational Observation
**Setiap link antar-server TCP di codebase ini tidak punya integrity/authentication.** `CheckNS()` (per-packet "network surveillance" hook) hard-coded `return true` di semua 4 trusted-node class:

- `MainServerNode::CheckNS` → `MainServerNode/MainServerNode.cpp:208` (`return true; //네트웍 감시 필요없다.`)
- `BillingRelayServer::CheckNS` → `BillingRelayServer/BillingRelayServer.cpp:135` (`return true;`)
- `ServerNode::CheckNS` → `NodeInfo/ServerNode.cpp:1000` (`return true;`)
- `DBAgentNode::CheckNS` → `DataBase/DBClient.cpp:67` (`return true;`)

Tidak ada shared secret, HMAC, sequence number, TLS, peer-identity verification. Setiap packet ID switch di link ini process attacker-controllable data jika attacker reach listening port atau inject stream. "Trusted internal links" threat model = effectively raw internet-facing listener.

---

## A. Authentication / Integrity Link

### IS-A1 — CRITICAL: MainServer link no auth/integrity
- **Lokasi:** `MainServerNode.cpp:208-211` (`CheckNS`), `:315-319` (`ReceivePacket`), `:320-761` (`PacketParsing`)
- **Root cause:** `CheckNS` return `true` unconditional; `PacketParsing` dispatch ~80 `MSTPK_*` handler no per-link auth state. Game server dial OUT ke main (`ConnectTo` `:104`), tapi inbound packet dari sana accepted verbatim.
- **PoC:** Attacker MITM (atau replace) main-server TCP (plaintext, unauth), atau reach main-server process & inject 1 `MSTPK_*`. Setiap finding B/C/D/E/F/I reachable.
- **Fix:** Handshake di link establish (HMAC nonce dgn per-deployment shared key), authenticated packet MAC, reject packet yang source node-type tidak match registered peer.

### IS-A2 — CRITICAL: BillingRelay link no auth/integrity
- **Lokasi:** `BillingRelayServer.cpp:135-138`, `:187-391` (`OnBillingPacketParsing`)
- **Root cause:** `CheckNS` return `true`. `OnBillingPacketParsing` dispatch cash/mileage/OTP/PCRoom/subscription result ke user purely by `dwUserIndex`/`szPrivateID`.
- **PoC:** Attacker di internal network reach billing-relay port (atau redirect via A4/`OnNodeInfoResponse`) inject `BSTPK_ADD_CASH_RESULT`/`BSTPK_OUTPUT_CASH_RESULT`/`BSTPK_GET_CASH_RESULT` addressed ke user index mana saja.
- **Fix:** Sama A1; billing GUID check (C1) partial guard & insufficient sendiri.

### IS-A3 — CRITICAL: Game-to-game SS port accept arbitrary connection no handshake
- **Lokasi:** `Network/GameServer.cpp:31-34` (`ioServerBind` `AcceptorServerNode` + `ITPK_ACCEPT_SESSION`); `AcceptorServerNode.cpp:48-64` (`OnAccept`); `ServerNode.cpp:1672-1695` (`OnConnectInfo`), `:1000-1003` (`CheckNS`)
- **Root cause:** `ServerBindStart` (`MainProcess.cpp:1591-1599`) bind SS port di private IP. `AcceptorServerNode::OnAccept` create `ServerNode` untuk socket mana saja yang kirim `ITPK_ACCEPT_SESSION`. `ServerNode::OnConnectInfo` trust peer-supplied `m_dwServerIndex`/`m_szServerIP`/`m_szClientMoveIP` zero verification. `CheckNS` return `true`.
- **PoC:** Host mana saja yang route ke SS port connect, kirim `ITPK_ACCEPT_SESSION`, lalu `SSTPK_CONNECT_INFO` claim server index arbitrary, lalu `SSTPK_*` packet mana saja (present insert, guild master change, trade complete, user move, dll). Bypass main-server link entirely.
- **Fix:** Mutual challenge-response auth di `SSTPK_CONNECT_INFO`; reject `ServerNode` yang claimed index/IP tidak di signed server roster obtained saat main-server connect.

### IS-A4 — CRITICAL: DBAgent link also unauthenticated (enabler, extends blast radius)
- **Lokasi:** `DataBase/DBClient.cpp:67-70` (`DBAgentNode::CheckNS`)
- Kombinasi B3 (`OnNodeInfoResponse` redirect), compromised main server bisa point game server ke attacker-controlled DB agent yang return forged query result (cash, inventory, login state). Lihat file 05 (DB-1/DB-2/DB-11).

---

## B. Shutdown / License / Process-Control Plane

### IS-B1 — HIGH: Forged `MSTPK_GAMESERVER_QUICK_EXIT`/`SAFETY_EXIT` shutdown server
- **Lokasi:** `MainServerNode.cpp:555-571`
- **Root cause:** `PacketParsing` langsung `g_App.Shutdown(SHUTDOWN_QUICK,1)`/`Shutdown(SHUTDOWN_SAFE,2)` di ID ini no auth.
- **PoC:** Single forged `MSTPK_GAMESERVER_QUICK_EXIT` packet di main-server link → entire game server process exit.
- **Fix:** Require authenticated shutdown token; rate-limit; restrict dedicated admin channel dgn own MAC.

### IS-B2 — HIGH: `MSTPK_LOW_CONNECT_SERVER_EXIT` trigger safe shutdown
- **Lokasi:** `MainServerNode.cpp:1447-1461`
- **Root cause:** Read `iServerConnect`, call `g_CriticalError.CheckServerDownMsg` & `g_App.Shutdown(SHUTDOWN_SAFE,9)`. Guard tunggal 5-minute uptime window (`TIMEGETTIME() < 300000`); setelah 5 min forged packet mana saja shutdown.
- **Fix:** Authenticate signal & verify vs locally-tracked peer count bukan trust value.

### IS-B3 — MEDIUM: License shutdown over UDP hardcoded key + hardcoded source IP (spoofable)
- **Lokasi:** `NodeInfo/LicenseManager.h:4-6` (`SHUTDOWN_KEY`, `LICENSE_SERVERIP`, `LICENSE_SERVERPORT`); `LicenseManager.cpp:28-61` (`OnLicense`); `MainProcess.cpp:1411-1415` (`LUPK_SHUTDOWN` dispatch)
- Lihat X-F15 file 09. `OnLicense` hanya accept jika `szShutDownKey == SHUTDOWN_KEY` (literal binary) & `sender IP == "210.118.58.224"`. Key recoverable dari binary; IP check bypassable on-path attacker spoof UDP source (no reply needed).
- **Fix:** Properly authenticated, replay-protected, crypto license protocol over established channel; jangan embed plaintext shared secret binary.

### IS-B4 — LOW: `MSTPK_LICENSE_REQUEST` (overseas) force license check/announcement
- **Lokasi:** `MainServerNode.cpp:752-754`, `:3408-3413`; `MainProcess.cpp:1957-1998`
- `OnRequestLicense` call `g_App.CheckLicenseForDev()` (compare local date ke local license date) & kirim `MSTPK_LICENSE_ALERT_CHECK` ke main. Mostly informational; forged request merely cause alert.
- **Fix:** Bagian A1.

---

## C. Cash / Billing Plane

### IS-C1 — HIGH: Forged billing result grant arbitrary cash; `SetCash`/`AddCash` no bounds (vs peso)
- **Lokasi:** `BillingRelayServer.cpp:328-391` (dispatch); `NodeInfo/User.cpp:4628-4636` (`SetCash`/`AddCash`); `:23963-23998` (`OnBillingAddCash`); `:20912` (`OnBillingGetCash`)
- **Root cause:** (1) Billing link unauth (A2). (2) `User::SetCash(int)` & `User::AddCash(int)` **tidak** apply `>10,000,000`/`>100,000,000` clamp-and-disconnect yang `SetMoney`/`AddMoney` apply (`User.cpp:4533-4573`); cash set/add no validation. (3) `OnBillingAddCash` cek `GetBillingGUID() == szBillingGUID` = partial guard — tapi GUID sendiri delivered via link unauth sama, & compromised/redirected billing server tahu itu.
- **PoC:** Attacker redirect billing link (B3/A4) capture user `BillingGUID` dari traffic, lalu kirim `BSTPK_ADD_CASH_RESULT { dwUserIndex, szBillingGUID, <large cash> }`. User `m_cash` set ke arbitrary no clamp.
- **Fix:** Authenticate billing link; add explicit bounds/clamp `SetCash`/`AddCash` mirror `SetMoney`; treat cash change dari billing authoritative hanya setelah end-to-end signed receipt.

### IS-C2 — MEDIUM: `BSTPK_REQUEST_USERINFO` dari billing dump semua connected user PII ke peer
- **Lokasi:** `BillingRelayServer.cpp:200-203` (dispatch), `:412-475` (`SendUserInfo`); `:104-108` (`OnCreate` kirim local IP/port/server index pertama)
- **Root cause:** Saat reconnect (`m_bReconnectState`), `SendUserInfo` iterasi entire user roster & write `channelingType, userIndex, channelID, publicIP, privateIP, privateID, server index` balik ke whoever kirim `BSTPK_REQUEST_USERINFO`. Recipient tidak authenticated.
- **PoC:** Attacker establish fake billing server (atau redirect via A4) & kirim `BSTPK_REQUEST_USERINFO`; game server reply PII setiap online user.
- **Fix:** Authenticate billing peer sebelum send roster; send hanya hashed/opaque ID ke relay.

### IS-C3 — MEDIUM: Billing reconnect loop re-entrant & pakai `static` buffer
- **Lokasi:** `BillingRelayServer.cpp:418-475`
- `static vUser vUserInfos;` reused; `while(1)` loop bound `userMax` dari `GetNodeSize()` tapi `userCount` hanya advance untuk user actually-returned. Jika manager size & actual retrievable set diverge → loop iterasi lebih dari perlu; `static` vector shared jika `SendUserInfo` ever re-entered.
- **Fix:** `vUserInfos` non-static local; bound loop strictly by returned count.

---

## D. Trade Flow (Main-Server–Coordinated)

Trade plane fundamental split-brained: game server ambil **buyer**'s peso & **seller** dapat proceeds/item sebagai **present**. Tidak ada handler verify trade listing actually exist, belong ke buyer/seller named, atau price match.

### IS-D1 — HIGH: Forged "trade sold" drain victim buyer peso & delete arbitrary listing
- **Lokasi:** `MainServerNode.cpp:1513-1616` (`OnTradeItemComplete`)
- **Root cause:** Saat `iResult == TRADE_ITEM_TRADE_OK`, handler lookup `dwBuyUserIndex` & jika local original user, compute `iResultPeso = iItemPrice + tax` & call `pBuyUser->RemoveMoney(iResultPeso)` (`:1563`), lalu issue `g_DBClient.OnTradeItemComplete(...)` (`:1571`) delete trade item di DB. **Tidak** cek:
  - buyer ever initiate purchase,
  - `dwTradeIndex` real listing,
  - item/price di packet match real listing,
  - `dwRegisterUserIndex` actually own listing,
  - price dalam sane bound (negative `iItemPrice` flow ke `RemoveMoney` yang hanya guard `< 0`, jadi *huge positive* forged price drain victim).
- **PoC:** Attacker di main-server link kirim `MSTPK_TRADE_ITEM_TRADE { TRADE_ITEM_TRADE_OK, dwBuyUserIndex=<victim>, dwTradeIndex=<any>, dwRegisterUserIndex=<any>, ..., iItemPrice=<huge> }`. Victim peso zeroed (`RemoveMoney` clamp 0) & DB delete issued untuk `dwTradeIndex` arbitrary (bisa destroy listing player lain).
- **Fix:** Maintain server-side in-memory record outstanding trade-buy intent keyed by `(buyerIndex, tradeIndex)`; require matching pending request sebelum deduct; cross-check price vs live trade listing (game server punya via `OnTradeGameSvrSync` `MainServerNode.cpp:3102`).

### IS-D2 — HIGH: Forged seller-side trade complete grant present (item/peso) ke user arbitrary
- **Lokasi:** `NodeInfo/ServerNode.cpp:6157-6185` (`OnTradeItemComplete`), `:6273-6325` (`OnTradeBuyComplete`), `:6327-6368` (`OnTradeSellComplete`), `:6507-6568` (`OnTradeTimeOut`); `ioPresentHelper.cpp:2526` (`SendPresentByTradeItemBuy`), `:2565` (`SendPresentByTradeItemSell`)
- **Root cause:** Handler `SSTPK_TRADE_*` (arrive via **unauth** game-to-game SS link A3) ambil `dwUserIndex`, lookup user, lalu call `g_PresentHelper.SendPresentByTradeItemBuy/Sell/TimeOut(...)` dgn attacker-supplied `dwItemType/dwItemMagicCode/dwItemValue/dwItemMaleCustom/dwItemFemaleCustom/szRegisterUserName` & attacker-supplied `iTradePrice` (sell). Present di-insert ke user mailbox & `pUser->_OnSelectPresent(30)` pull ke client. Tidak verify trade ever exist di main server.
- **PoC:** Attacker connect SS port (A3), claim server index, kirim `SSTPK_TRADE_ITEM_COMPLETE { dwUserIndex=<accomplice>, TRADE_S_BUY_COMPLETE, <crafted item fields> }`. Accomplice terima item sebagai present — item duplication / creation out of thin air.
- **Fix:** Cross-validate setiap seller-side trade completion vs locally synced trade list (`g_TradeSyncMgr`); reject completion untuk `dwTradeIndex` tidak present di synced list atau field tidak match.

### IS-D3 — MEDIUM: Forged trade timeout return item ke user arbitrary
- **Lokasi:** `MainServerNode.cpp:1779-1870` (`OnTradeItemTimeOut`)
- **Root cause:** Di timeout branch no `pRegUserParent` present, handler tetap call `g_PresentHelper.SendPresentByTradeTimeOut(0, 0, dwRegisterUserIndex, dwRegisterUserIndex, ...)` (`:1850`) — kirim present ke `dwRegisterUserIndex` dgn attacker-supplied item field & **no logged-in-user requirement**. Bahkan in-server branch (`:1798`) hanya check `GetGlobalUserNode`, bukan listing exist.
- **PoC:** Forge `MSTPK_TRADE_TIME_OUT { dwRegisterUserIndex=<accomplice>, <valuable item fields> }` saat accomplice online/offline; accomplice terima item.
- **Fix:** Validate vs synced trade list; require trade index exist.

### IS-D4 — MEDIUM: Forged trade cancel delete arbitrary listing / return item
- **Lokasi:** `MainServerNode.cpp:1664-1777` (`OnTradeItemCancel`)
- **Root cause:** `OnTradeItemCancel` issue `g_DBClient.OnTradeItemCancel(...)` (`:1692`) untuk attacker-supplied `dwRegisterUserIndex`/`dwTradeIndex`/item field. No ownership check.
- **PoC:** Forge `MSTPK_TRADE_ITEM_CANCEL { TRADE_ITEM_CANCEL_OK, dwRegisterUserIndex=<victim>, dwTradeIndex=<victim's listing>, ... }` delete victim listing & item return ke mereka (atau accomplice via timeout/present path).
- **Fix:** Sama D1/D2 — validate vs synced trade list.

### IS-D5 — LOW: `MSTPK_TRADE_ITEM_GAMESVR_SYNC` (`OnTradeGameSvrSync`) itself unauth feed
- **Lokasi:** `MainServerNode.cpp:3102-3135`; `NodeInfo/TradeSyncManger.cpp`
- **Root cause:** Whole trade listing table game server populated dari `MSTPK_TRADE_ITEM_GAMESVR_SYNC` packet (`TRADE_ALL/ADD/DEL`). Karena main-server link unauth (A1), attacker corrupt/empty game server trade table at will, yang lalu affect validation suggested D1/D2.
- **Fix:** Authenticated sync (A1) + sequence/replay protection.

---

## E. Present Plane

### IS-E1 — CRITICAL: `SSTPK_PRESENT_INSERT`/`SSTPK_PRESENT_INSERT_BY_ETC_ITEM` grant arbitrary present ke user logged-in mana saja via game-to-game link
- **Lokasi:** `NodeInfo/ServerNode.cpp:6802-6821` (`OnPresentInsert`), `:6823-6859` (`OnPresentInsertByEtcItem`); dispatch `:1428-1429` / `:1441-1442`
- **Root cause:** Handler run di `ServerNode` (**unauth** SS-link A3). Ambil `dwUserIndex`, resolve user, read attacker-supplied `kSendID, iPresentType, iPresentValue1..4, iPresentMent, iPresentDay, iPresentState, kLogMent`, lalu call `pUser->AddPresentMemory(...)` & `pUser->SendPresentMemory()`. Zero validation `iPresentType`/value — any item type, any quantity, any value, ke user mana saja, attributed ke forged `kSendID`.
- **PoC:** Attacker connect SS port (A3), kirim `SSTPK_PRESENT_INSERT { dwUserIndex=<any online user>, iPresentType=<cash/item/etc>, iPresentValue1..4=<arbitrary> }`. User langsung terima present di mailbox.
- **Fix:** `SSTPK_PRESENT_*` family **tidak boleh** accept di game-to-game link; present hanya originate dari DB/Main server setelah validation. Minimal require authenticated, per-present signed token dari main server.

### IS-E2 — CRITICAL: `MSTPK_ADMIN`/`ADMINCOMMAND_ITEMINSERT` grant arbitrary present ke user (online/offline)
- **Lokasi:** `MainServerNode.cpp:1118-1145` (`OnAdminCommand`), `:914-1070` (`OnAdminItemInsert`)
- **Root cause:** `MSTPK_ADMIN` arrive via unauth main-server link (A1). `OnAdminItemInsert` read `szUserID, iPresentType, iPresentValue1, iPresentValue2, iPresentMent, iPublcIDState` & call `g_DBClient.OnPresentInsertByPrivateID(...)`/`OnInsertPresentData(...)` langsung ke DB. Target user **tidak** perlu online (`dwAgentID`/`dwUserIndex` dari lookup yang mungkin return NULL & insert tetap proceed `:977-1004`). No authorization check, no item-type whitelist, no value bound.
- **PoC:** Forge `MSTPK_ADMIN { ADMINCOMMAND_ITEMINSERT, szUserID=<any account>, iPresentType=<cash/etc>, iPresentValue1=<large> }`. Present di-insert ke DB untuk account named regardless online status.
- **Fix:** Authenticate admin command (A1); enforce server-side admin ACL & value/type whitelist; require signed admin token bukan trust main-server stream.

---

## F. Guild Plane

Semua guild mutation arrive sebagai `SSTPK_GUILD_*` (via unauth game-to-game SS link A3) atau `MSTPK_GUILD_*` (via unauth main-server link A1). Tiap trust `dwUserIndex`/`dwGuildIndex` blind & issue DB write.

### IS-F1 — HIGH: `SSTPK_GUILD_MASTER_CHANGE` transfer guild master ke user arbitrary
- **Lokasi:** `ServerNode.cpp:4823-4866`
- **Root cause:** Read `dwUserIndex, dwTargetIndex, dwGuildIndex`; jika target current `pUserGuild->GetGuildIndex() == dwGuildIndex`, call `g_DBClient.OnUpdateGuildMasterChange(...)` (`:4852`) set attacker-named target sebagai master. No verify `dwUserIndex` current master atau main server authorize change.
- **PoC:** Forged SS connection: `SSTPK_GUILD_MASTER_CHANGE { dwUserIndex=<any>, dwTargetIndex=<attacker>, dwGuildIndex=<target guild> }` → attacker jadi guild master di DB.
- **Fix:** Guild master change authorize hanya oleh main server guild service setelah verify requester; game server tidak mutate guild leadership berdasarkan peer-server packet sendiri.

### IS-F2 — HIGH: `SSTPK_GUILD_KICK_OUT` remove user arbitrary dari guild
- **Lokasi:** `ServerNode.cpp:4887-4931`
- **Root cause:** Saat `pUserGuild->GetGuildIndex() == dwGuildIndex`, call `g_DBClient.OnDeleteGuildLeaveUser(...)` (`:4905`) & `g_MainServer.SendMessage(MSTPK_GUILD_LEAVE)` (`:4910`). No check `dwSendIndex` authorized.
- **Fix:** Sama F1.

### IS-F3 — HIGH: `SSTPK_CREATE_GUILD_RESULT`/`SSTPK_CREATE_GUILD_COMPLETE` forge guild creation & set guild data
- **Lokasi:** `ServerNode.cpp:4596-4643` (`OnCreateGuildResult`), `:4645-4688` (`OnCreateGuildComplete`)
- **Root cause:** `OnCreateGuildResult` dgn `CREATE_GUILD_OK` trigger `OnSelectCreateGuildInfo` & delete guild-create etc-item dari user (`:4624-4628`). `OnCreateGuildComplete` call `pUserGuild->SetGuildData(dwGuildIndex, szGuildName, szGuildPos, iGuildMark, true)` dgn semua attacker-supplied field (`:4682`), lalu kirim `STPK_CREATE_GUILD` ke client. No verify user bayar/request creation.
- **PoC:** Forge `SSTPK_CREATE_GUILD_COMPLETE { dwUserIndex=<accomplice>, dwGuildIndex=<picked>, szGuildName=<...>, ... }` → accomplice jadi guild master dgn forged guild identity (& create-item consume free).
- **Fix:** Authorize creation via main server; jangan accept guild-creation "success" dari peer game server.

### IS-F4 — HIGH: `SSTPK_GUILD_ENTRY_AGREE` force user ke guild
- **Lokasi:** `ServerNode.cpp:4690-4754`
- **Root cause:** Call `pUserGuild->SetGuildDataEntryAgree(dwGuildIndex, ...)` & `g_DBClient.OnUpdateGuildMemberEvent(...)`/`OnSelectGuildMemberListEx(...)` (`:4751-4752`) dgn attacker-supplied `dwGuildIndex`. No confirmation user agree.
- **Fix:** Sama F1.

### IS-F5 — MEDIUM: `SSTPK_GUILD_POSITION_CHANGE`/`SSTPK_GUILD_MARK_CHANGE`/`SSTPK_GUILD_USER_DELETE`/`SSTPK_GUILD_NAME_CHANGE*`
- **Lokasi:** `ServerNode.cpp:4868-4885` (position), `:4954-4969` (mark), `:4971-5004` (user delete), `:5560-5603` (name change)
- **Root cause:** Tiap read `dwUserIndex` + attacker field & mutate `ioUserGuild`/DB no authorization.
- **Fix:** Sama F1.

### IS-F6 — HIGH: `MSTPK_GUILD_INFO` (main server) set guild level & trigger rank reward
- **Lokasi:** `MainServerNode.cpp:1192-1229`
- **Root cause:** Read `bLoginInfo, dwUserIndex, dwGuildIndex, dwGuildMark, dwGuildRank, dwGuildLevel` & saat `bLoginInfo`, call `pUserGuild->SetGuildLevel(dwGuildLevel)` & `pUserGuild->DoRecvGuildRankReward()` (`:1222-1223`) — grant guild-rank reward ke user mana saja berdasarkan forged `dwGuildLevel`.
- **PoC:** Forge `MSTPK_GUILD_INFO { bLoginInfo=true, dwUserIndex=<accomplice>, dwGuildLevel=<max> }` → accomplice terima rank reward.
- **Fix:** Authenticate main-server link; game server track authoritative guild level & reject out-of-band increase.

### IS-F7 — MEDIUM: `MSTPK_GUILD_CHANGE_JOINER`/`MSTPK_GUILD_EXIST`/`MSTPK_GUILD_SIMPLE_INFO`/`MSTPK_GUILD_TITLE_SYNC`/`MSTPK_GUILD_MARK_BLOCK_INFO`
- **Lokasi:** `MainServerNode.cpp:1231-1247`, `:1301-1336`, `:1315-1322`, `:149-150` declaration
- **Root cause:** Mostly relay ke client, tapi `OnGuildChangeJoiner` relay `dwGuildMaxUser` (capacity) & `OnGuildMarkBlockInfo` invoke `g_DBClient.OnSelectGuildMarkBlockInfo` dgn forged `szDeveloperID` (`:1321`) — enable attacker query/trigger DB lookup untuk arbitrary developer ID (DB-side load / info leak).
- **Fix:** Authenticate; validate `szDeveloperID` format & request origin.

---

## G. Cross-Server Copy-Node Sync (Game-to-Game)

### IS-G1 — HIGH: Attacker-controlled unbounded `iMaxBestFriend` loop (memory growth / unbounded work)
- **Lokasi:** `NodeInfo/UserCopyNode.cpp:65-81` (`ApplySyncCreate`), `:113-124` (`ApplySyncBestFriend`)
- **Root cause:** `rkPacket >> iMaxBestFriend;` lalu `for(int i=0;i<iMaxBestFriend;i++) m_vBestFriend.push_back(dwBestFriendIndex);`. `iMaxBestFriend` straight dari peer packet no upper bound. Single large value grow vector arbitrary; value larger than remaining packet trigger failed read tapi loop continue `iMaxBestFriend` iteration regardless.
- **PoC:** Forged `SSTPK_CONNECT_SYNC` (atau sync carrying `ApplySyncCreate`) `iMaxBestFriend = 0x7FFFFFFF` → game server alloc/push miliaran entry (crash/OOM) atau burn CPU.
- **Fix:** Cap `iMaxBestFriend` ke sane constant (mis. `FRIEND_LIST_MOVE_COUNT` = 50, `ServerNode.h:12`); break out jika packet read fail.

### IS-G2 — MEDIUM: `OnConnectSync` create copy node dgn attacker-controlled index (collision / garbage state)
- **Lokasi:** `NodeInfo/ServerNode.cpp:1697-1832`
- **Root cause:** Tiap sync sub-type read `iSize` lalu loop `iSize` kali, read index & call `CreateNewUser/Room/BattleRoom/Channel/LadderTeam/ShuffleRoom(index)`. `CreateNew*` pertama check existing node index itu & **return** jika present (`:107-112`, `:248-252`, dll). Forged sync claim index apa saja, termasuk index sudah dipakai copy node lain atau real local node, cause existing node overwrite/re-apply dgn attacker data (`ApplySyncCreate`), corrupt room/user count & lookup cross manager.
- **PoC:** Forge `SSTPK_CONNECT_SYNC { SSTPK_CONNECT_SYNC_USER, iSize=1, dwUserIndex=<existing copy node's index>, <crafted fields> }` overwrite legitimate copy node IP/publicID/etc.
- **Fix:** Verify connecting peer authoritative owner claimed index (tiap index must belong peer's server, recorded saat `OnConnectInfo`); reject duplikat dari peer beda.

### IS-G3 — HIGH: `OnAllServerList` inject arbitrary game-server topology & force outbound connect
- **Lokasi:** `MainServerNode.cpp:818-871`
- **Root cause:** `OnAllServerList` read `iServerListSize` lalu tiap entry read `dwServerIndex, szServerIP, iSSPort` & call `g_ServerNodeManager.ConnectTo(dwServerIndex, szServerIP.c_str(), iSSPort)` (`:844`). Game server dial **any** IP:port yang (unauth) main server supply. First connect fail → server shutdown (`:846-850). `iServerListSize` unbounded (only bounded by packet data), & entry inserted tanpa verify IP belong deployment.
- **PoC:** Compromised main server kirim list berisi attacker-controlled IP → game server connect ke attacker node (yang lalu jadi `ServerNode` peer & inject semua `SSTPK_*` flow F/E/D). Alternatif kirim 1 bad entry force shutdown.
- **Fix:** Validate tiap supplied IP vs signed server roster; bound `iServerListSize`; jangan shutdown single peer-connect failure.

### IS-G4 — HIGH: `OnNodeInfoResponse` redirect DB/Billing/LogDB/LogServer/Wemade endpoint ke attacker IP
- **Lokasi:** `MainServerNode.cpp:2739-2866`
- **Root cause:** Main server supply IP:port list untuk BillingRelay, LogDBAgent, GameDBAgent, LogServer, Wemade logger. Game server store verbatim (`g_BillingRelayServer.SetBillingServerInfo`, `g_LogDBClient.SetLOGDBAgentInfo`, `g_DBClient.AddDBAgentInfo`, `g_App.SetLogServer`, `g_WemadeLogger.Register`) lalu **connect out** (`g_DBClient.ConnectTo()` `:2861`). Kombinasi A2/A4 (DBAgent & billing `CheckNS==true`) → forged `MSTPK_NODEINFO_REQUEST` response buat game server send semua DB query & user billing traffic ke attacker-controlled host — leak setiap query/result & enable forged DB/billing response (cash, inventory, login state).
- **PoC:** MITM/replace main-server link, kirim `MSTPK_NODEINFO_REQUEST` response dgn attacker IP untuk DB agent list. `OnServerIndex`/reconnect berikut game server connect ke attacker DB.
- **Fix:** Pin endpoint list di deploy time (config file, bukan over wire), atau sign dgn key established first boot; jangan accept endpoint change dari unauth stream.

---

## H. Memory Leaks / Unbounded Growth

### IS-H1 — LOW: `m_vVersions` grow unbounded via repeated `MSTPK_UPDATE_CLIENT_VERSION`
- **Lokasi:** `MainServerNode.cpp:1263-1286` (`OnUpdateClientVersion`), `:111` (`IsUseClientVersion`)
- **Root cause:** Tiap `MSTPK_UPDATE_CLIENT_VERSION` dgn `bUseClientVersion=true` `m_vVersions.push_back(iClientVersion)` (`:1277`) no dedup/size cap. `IsRightClientVersion` linear `std::find` (`:1293`) → unbounded growth degrade login-time CPU.
- **PoC:** Flood main-server link dgn version packet → memory grow & setiap login O(n).
- **Fix:** Cap ke small set atau replace dgn hash set.

### IS-H2 — MEDIUM: `m_ConnectWorkingPacket` queue per `ServerNode` buffer setiap pre-sync packet unbounded
- **Lokasi:** `NodeInfo/ServerNode.cpp:1597-1616` (`OnConnectWorkingPacket`), `:1812-1828` (replay)
- **Root cause:** Saat `m_bConnectWorkComplete` false, setiap non-control packet di-push ke `m_ConnectWorkingPacket` (`:1614`). Malicious peer (A3) connect, tidak pernah kirim `SSTPK_CONNECT_SYNC_COMPLETE`, & flood queue. Vector hanya clear saat completion atau session destroy.
- **Fix:** Bound queue; disconnect peer yang tidak complete sync dalam timeout.

### IS-H3 — LOW: `MainServerNode`/`BillingRelayServer` singleton tidak free sampai `ReleaseInstance`
- **Lokasi:** `MainServerNode.cpp:80-97`, `BillingRelayServer.cpp:33-46`
- Process-lifetime singleton. Bukan leak per se, tapi `ConnectTo()` create new socket tiap reconnect; ensure prior socket/handle fully released (`OnCreate`/`AfterCreate` di fresh socket tanpa explicit close prior — verify `CConnectNode::SetSocket` handle; jika tidak → handle leak repeated reconnect).
- **Fix:** Audit `ConnectTo` re-entry guarantee previous socket closed sebelum alloc baru.

### IS-H4 — LOW: `static vUser vUserInfos` di `BillingRelayServer::SendUserInfo`
- **Lokasi:** `BillingRelayServer.cpp:418`
- `static vUser vUserInfos;` reused across call; jika 2 billing-request packet concurrent (atau re-entrant) shared vector corrupt. Minor leak risk jika `clear()` skip di early return path.
- **Fix:** Plain local.

---

## I. Other Unauthenticated Main-Server Control Flow

### IS-I1 — MEDIUM: `MSTPK_CAMP_BATTLE_INFO` toggle global camp/ladder state & trigger mass ladder update
- **Lokasi:** `MainServerNode.cpp:1413-1433`
- **Root cause:** Saat `bBattlePaly=false`, call `g_UserNodeManager.AllUserUpdateLadderPointNExpert()` (`:1426`) — heavy per-user operation seluruh server — & saat `true` call `AllCampUserCampDataSync()`. Forged packet force server-wide ladder update (DoS) atau flip camp state.
- **Fix:** Authenticate (A1); cross-check vs local camp schedule.

### IS-I2 — MEDIUM: `MSTPK_GAMESERVER_CHANGE_RELAYSERVER_STATE`/`MSTPK_WHITELIST_REQUEST`/`MSTPK_EVENT_NPC_CLOSE_REQUEST`
- **Lokasi:** `MainServerNode.cpp:2868-2877` (relay toggle), `:2702-2708` (whitelist), `:2710-2723` (NPC)
- **Root cause:** `OnWhiteListRequest` flip IP whitelist (`g_IPBlock.SetWhiteList(bWhiteList)`) — forged `bWhiteList=FALSE` disable whitelist. `OnRelayServerChangeState` toggle relay usage global. `OnNpcEventRequest` toggle NPC spawn. None authenticated.
- **Fix:** A1; restrict ke signed admin sub-channel.

### IS-I3 — MEDIUM: `MSTPK_UPDATE_RELATIVE_GRADE` force server-wide recompute
- **Lokasi:** `MainServerNode.cpp:2725-2731`
- **Root cause:** Read `dwUniqueCode` & call `g_UserNodeManager.UpdateRelativeGradeAllUser(dwUniqueCode)` — recompute relative grade setiap user. Repeated forgery = sustained CPU load.
- **Fix:** A1; rate-limit; validate `dwUniqueCode` monotonicity.

### IS-I4 — MEDIUM: `MSTPK_ADMIN` family: kick/announce/event-insert/user-block
- **Lokasi:** `MainServerNode.cpp:1118-1145` (`OnAdminCommand`), `:892-912` (kick), `:901-912` (announce), `:1072-1116` (event/userblock)
- **Root cause:** `ADMINCOMMAND_KICK` disconnect any `szUserID`; `ADMINCOMMAND_ANNOUNCE` inject any announcement ke all/any user; `ADMINCOMMAND_EVENTINSERT` mutate global event state (dan BOF S-C2); `ADMINCOMMAND_USERBLOCK` set arbitrary block level ke user & relay.
- **Fix:** A1 + admin ACL.

### IS-I5 — CRITICAL: `OnAdminEventInsert` stack buffer overflow (`iValues[64]` written dgn unbounded `iValueCount`)
- **Lokasi:** `MainServerNode.cpp:1072-1086`
- Lihat S-C2 file 08. Stack BOF → RCE dari link main unauth. Single most urgent concrete bug di file ini regardless broader auth problem.
- **Fix:** `if(iValueCount > 64) return;` (atau size `iValues` dynamic & cap small).

### IS-I6 — LOW: `MSTPK_UPDATE_CLIENT_VERSION` echo `szGUID`/version ke main (minor info disclosure/reflection)
- **Lokasi:** `MainServerNode.cpp:1263-1286`
- Kirim `MSTPK_UPDATE_CLIENT_VERSION_RESULT` dgn attacker-supplied `szGUID`/`iClientVersion`. Negligible alone, tapi primitive untuk launder data ke main server / monitoring tool.
- **Fix:** A1.

---

## Cross-Cutting Recommendation
Single highest-leverage fix = **A1–A4**: introduce real auth + integrity layer (mutual challenge-response dgn per-deployment shared secret established first provisioning, + per-packet HMAC + monotonic sequence) di **semua 4** trusted-node class. Sampai itu ada, setiap "trusted" handler — termasuk admin command, present injection, guild mutation, trade completion, cash grant, endpoint redirect, shutdown — effectively unauth remote control game server. Most urgent concrete code bug independent of auth = **I5** (`iValues[64]` stack overflow), directly exploitable RCE dari compromised main-server link.

---

## Prioritas Fix Lokal
1. **IS-A1..A4 (P0):** HMAC+nonce+sequence+TLS semua link inter-server.
2. **IS-I5 (P0):** `if(iValueCount > 64) return;`.
3. **IS-E1/E2 (P0):** Tolak present/admin inject di link unauth.
4. **IS-B1/B2 (P0):** Auth shutdown token.
5. **IS-D1/D2/F1..F4/G1/G3/G4 (P1):** Cross-validate vs synced trade/guild list; validate server roster.
6. **IS-C1 (P1):** Clamp `SetCash/AddCash`.
