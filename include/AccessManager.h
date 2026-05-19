#pragma once
#include <Arduino.h>
#include "Config.h"

// ================================================================
// AccessManager  —  State login / logout / lock
//
// Menyimpan siapa yang sedang login dan status kunci ruangan.
// Tidak berisi logika hardware — hanya state.
// ================================================================

class AccessManager {
public:
  AccessManager()
    : _login(false), _locked(false), _modeAuto(true) {
    strcpy(_user, "none");
    strcpy(_uid,  "none");
  }

  // -------- Login / Logout --------

  void doLogin(const char* userName, const char* uid) {
    _login = true;
    strncpy(_user, userName, USER_NAME_SIZE - 1);
    _user[USER_NAME_SIZE - 1] = '\0';
    strncpy(_uid, uid, USER_ID_SIZE - 1);
    _uid[USER_ID_SIZE - 1] = '\0';
    Serial.printf("[ACCESS] Login: %s (%s)\n", _user, _uid);
  }

  void doLogout() {
    Serial.printf("[ACCESS] Logout: %s\n", _user);
    _login = false;
    strcpy(_user, "none");
    strcpy(_uid,  "none");
  }

  // -------- Lock --------
  void setLocked(bool state)   { _locked = state; }
  void setModeAuto(bool state) { _modeAuto = state; }

  // -------- Getters --------
  bool        isLoggedIn()  const { return _login; }
  bool        isLocked()    const { return _locked; }
  bool        isModeAuto()  const { return _modeAuto; }
  const char* getUser()     const { return _user; }
  const char* getUID()      const { return _uid; }

  // Cek apakah kartu yang ditap cocok dengan yang sedang login
  bool isCurrentUser(const char* uid) const {
    return strcmp(uid, _uid) == 0;
  }

private:
  bool _login, _locked, _modeAuto;
  char _user[USER_NAME_SIZE];
  char _uid[USER_ID_SIZE];
};
