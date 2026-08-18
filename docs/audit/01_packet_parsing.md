# 01 — Packet Parsing Layer (CPacket / SP2Packet)

**Area:** Lapisan paling bawah parsing paket, dipakai semua subsystem.
**File dibaca:** `iocpSocketDLL/SocketModules/Packet.h`, `Packet.cpp`, `Network/SP2Packet.h`, `SP2Packet.cpp`

> Ini adalah **akar masalah RC-1 & RC-2** yang mengaktifkan puluhan temuan di area lain. Temuan di sini adalah root cause, bukan instance tunggal.

---

## Temuan

### P-1 / RC-1 — CRITICAL: `operator>>(LPTSTR)` Stack Buffer Overflow Engine
- **Lokasi:** `iocpSocketDLL/SocketModules/Packet.cpp:291-305`
- **Tipe:** Stack buffer overflow (generic enabler)
- **Root cause:**
  ```cpp
  CPacket& CPacket::operator >> (LPTSTR arg) {
      int nlen = lstrlen((LPTSTR)&m_pBuffer[m_currentPos]) * sizeof(TCHAR) + sizeof(TCHAR);
      if( !CheckRightPacketSize( nlen ) ) { arg = NULL; return *this; }
      memcpy(arg, &m_pBuffer[m_currentPos], nlen);   // arg size UNKNOWN
      m_currentPos += nlen;
      return *this;
  }
  ```
  `arg` adalah buffer caller berukuran tetap (mis. `char szID[21]`). `lstrlen` scan null dari buffer `m_pBuffer[65536]`. `memcpy` tulis sepanjang `nlen` ke buffer tujuan **tanpa cek ukuran tujuan** → stack smash. Failure path `arg = NULL` cuma null-kan pointer lokal (tidak amankan data, dan tidak hentikan pemanggilan berikutnya). Setiap `packet >> charBuf[N]` di codebase berpotensi overflow.
- **PoC:** Lihat instance konkret: U-1 (`MainProcess.cpp:1401`), X-F31 (`EtcHelpFunc.cpp:613`), dan puluhan call site `>> char[N]` di NodeInfo/*. Serang dengan string null-terminated dalam paket lebih panjang dari buffer tujuan.
- **Fix:** Hapus `operator>>(LPTSTR)`. Paksa semua caller pakai `Read(LPTSTR arg, int size)` (yang ada length param) atau read ke `ioHashString` lalu validasi panjang sebelum copy.

### P-2 / RC-2 — MEDIUM: `CheckRightPacketSize` cek ke `MAX_BUFFER` bukan ukuran paket
- **Lokasi:** `Packet.cpp:118-127`
- **Root cause:**
  ```cpp
  bool CheckRightPacketSize( int iAddSize ) {
      if( m_currentPos + iAddSize >= MAX_BUFFER ) { ... return false; }
      return true;
  }
  ```
  `MAX_BUFFER` = 65536 (kapasitas buffer fisik), bukan `GetBufferSize()` (ukuran paket aktual dari header). Pembacaan bisa lewati data paket aktual ke region buffer yang di-zero/stale. Untuk TCP reuse koneksi, buffer mungkin berisi data paket sebelumnya.
- **PoC:** Kirim TCP paket dgn header size=50, string field di offset N tanpa null dalam 50 byte. `lstrlen` scan lewati byte 50 ke null berikutnya di 65536 buffer (data stale) → handler proses string lebih panjang dari payload asli.
- **Fix:** Tambah varian `CheckRightPacketSize` yang bound ke `GetBufferSize()` (header-declared size); pastikan semua read pakai varian itu.

### P-3 — MEDIUM: `lstrlen` pre-check di `ioHashString` & `Read(ioHashString)`
- **Lokasi:** `SP2Packet.cpp:519-532` (`>> ioHashString`), `:619-633`, `:1119-1133` (`Read(ioHashString)`), `:1149-1179` (`Read(LPTSTR,int)`)
- **Root cause:** Semua baca string compute `nlen = lstrlen(&m_pBuffer[m_currentPos])` **sebelum** `CheckRightPacketSize(nlen)`. Jika field string tanpa null dalam payload, `lstrlen` baca OOB ke buffer. `Read(LPTSTR,int)` aman dari sisi tujuan (ada length param) tapi `lstrlen` source tetap OOB.
- **PoC:** Paket dgn string field tanpa null dalam real payload → `lstrlen` heap OOB read → crash/info leak. Memengaruhi SEMUA handler yang baca `ioHashString` (OnDropDie, OnPassage, OnPrisonerMode, OnCatchChar, OnBattleRoomInvite, chat, dll).
- **Fix:** Scan null dgn `memchr` bounded ke `GetBufferSize() - m_currentPos`; jika tidak ada null dalam real payload, tolak paket. Ganti `lstrlen` dengan `strnlen_s` bounded.

### P-4 — LOW: `SetBufferCopy` asumsi header size
- **Lokasi:** `Packet.cpp:20-25,71-83`
- **Root cause:** `m_currentPos = sizeof(PACKETHEADER)` (16 byte) tapi `Clear()` set `*m_packet_header.m_Size = sizeof(PACKETHEADER)`. Jika buffer masuk lebih kecil dari 16, header field (`m_ID`/`m_Size`/`m_CheckSum`/`m_iState`) jadi garbage dari buffer. `IsValidHeader()` (`Packet.cpp:95-98`) cek `GetBufferSize() >= sizeof(PACKETHEADER)` tapi `SetBufferCopy` dipanggil sebelum validasi di beberapa path.
- **Fix:** Validasi `size >= PK_HEADER_SIZE` di `SetBufferCopy` sebelum set header pointer.

---

## Tambahan: Konstruktor `CPacket(char*, int)` clamp OK
- `Packet.cpp:20-25`: `memcpy(&m_pBuffer[0], buffer, min(MAX_BUFFER, size))` — clamp benar, tidak heap overflow. Tapi `m_currentPos = sizeof(PACKETHEADER)` tetap asumsi header penuh (P-4).

## Tambahan: `operator=(CPacket&)` copy tanpa clamp
- `Packet.cpp:130-137`: `memcpy(&m_pBuffer[0], packet.GetBuffer(), packet.GetBufferSize())`. `GetBufferSize()` = `*m_Size` (attacker-controlled). Jika source paket punya `m_Size > MAX_BUFFER` (tidak mungkin karena buffer source juga MAX_BUFFER) → aman. Tapi jika `m_Size` di-source di-forge lebih besar dari real data, copy garbage. Low impact karena source juga CPacket dgn buffer MAX_BUFFER.

---

## Prioritas Fix Lokal
1. **P-1 (P0):** Hapus `operator>>(LPTSTR)`. Ini menghentikan seluruh kelas BOF.
2. **P-2 (P1):** Bound check vs `GetBufferSize()`.
3. **P-3 (P1):** Bounded string scan dgn `memchr`.

*Lintas-area: instance konkret P-1 ada di 02 (U-1), 05 (DB-1), 09 (X-F31, X-F34).*
