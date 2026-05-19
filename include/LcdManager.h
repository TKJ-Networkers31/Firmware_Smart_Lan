#pragma once
#include <LiquidCrystal_I2C.h>

// ================================================================
// LcdManager  —  Wrapper LCD I2C 16x2
//
// Fitur utama:
//  - print() dengan auto-padding agar baris sebelumnya terhapus
//    TANPA harus lcd.clear() setiap saat (clear itu lambat & flicker)
//  - showTimed() → tampil sementara, lalu kembali ke pesan default
//  - update()    → dipanggil di loop(), handle kembali ke default
// ================================================================

class LcdManager {
public:
  LcdManager(uint8_t addr, uint8_t cols, uint8_t rows)
    : _lcd(addr, cols, rows), _cols(cols), _rows(rows),
      _timedUntil(0), _needRestore(false) {}

  void begin() {
    _lcd.init();
    _lcd.backlight();
    clearDisplay();
  }

  // Tampilkan 2 baris permanen (tanpa clear, pakai padding)
  void show(const char* line1, const char* line2 = "") {
    _printPadded(0, line1);
    _printPadded(1, line2);
    // Simpan sebagai default agar bisa di-restore
    strncpy(_defaultL1, line1, _cols);
    strncpy(_defaultL2, line2, _cols);
    _defaultL1[_cols] = '\0';
    _defaultL2[_cols] = '\0';
    _needRestore = false;
  }

  // Tampilkan sementara (ms), setelah itu kembali ke pesan default
  // BUG FIX: di kode lama, pesan akses diterima/ditolak langsung
  // ditimpa loop karena tidak ada mekanisme timed display
  void showTimed(const char* line1, const char* line2, unsigned long durationMs) {
    _printPadded(0, line1);
    _printPadded(1, line2);
    _timedUntil = millis() + durationMs;
    _needRestore = true;
  }

  // Set pesan default tanpa langsung menampilkannya
  void setDefault(const char* line1, const char* line2 = "") {
    strncpy(_defaultL1, line1, _cols);
    strncpy(_defaultL2, line2, _cols);
    _defaultL1[_cols] = '\0';
    _defaultL2[_cols] = '\0';
  }

  // Kembali paksa ke pesan default
  void restoreDefault() {
    _printPadded(0, _defaultL1);
    _printPadded(1, _defaultL2);
    _needRestore = false;
  }

  // Dipanggil setiap loop() — handle timeout timed display
  void update() {
    if (_needRestore && millis() >= _timedUntil) {
      restoreDefault();
    }
  }

  void clearDisplay() {
    _lcd.clear();
    memset(_defaultL1, ' ', _cols); _defaultL1[_cols] = '\0';
    memset(_defaultL2, ' ', _cols); _defaultL2[_cols] = '\0';
  }

  bool isBusy() const { return _needRestore && millis() < _timedUntil; }

private:
  LiquidCrystal_I2C _lcd;
  uint8_t _cols, _rows;
  char _defaultL1[17]; // 16 char + null
  char _defaultL2[17];
  unsigned long _timedUntil;
  bool _needRestore;

  // Print satu baris dengan padding spasi agar overwrite baris sebelumnya
  // tanpa lcd.clear() — ini menghilangkan flicker
  void _printPadded(uint8_t row, const char* text) {
    _lcd.setCursor(0, row);
    char buf[17];
    snprintf(buf, sizeof(buf), "%-16s", text); // left-align, pad kanan dengan spasi
    _lcd.print(buf);
  }
};
