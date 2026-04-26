#pragma once

// =============================================================================
//  Storage.hpp
//  Append-only CSV logger that mirrors each row to:
//    - microSD (when USE_SD == 1 and the card mounts)
//    - LittleFS in onboard flash (when USE_FLASH == 1)
//
//  Writes that fail are counted in failures(). The class never throws and
//  silently degrades: if SD vanishes mid-flight, the flash mirror keeps the
//  data so nothing is lost.
// =============================================================================

#include "Config.hpp"

class Storage {
public:
  // Input  : run - current run identifier, used in the flash filename.
  // What   : mounts SD and/or LittleFS, creates log directories, writes the
  //          CSV header into any newly-created file.
  // Output : true if at least one of the storage backends mounted.
  bool begin(uint32_t run);

  // Input  : line - one CSV-formatted record (no trailing newline).
  // What   : appends `line` to every active backend; updates the failure
  //          counter for any backend that errors out.
  // Output : (none)
  void writeRow(const String& line);

  // Input  : (none)
  // What   : prints every byte of every flash log file to Serial.
  //          Useful for retrieving stored data without removing the card.
  // Output : (none)
  void dumpFlashToSerial();

  // Input  : (none)
  // What   : deletes every file under FLASH_LOG_DIR and recreates the
  //          header row in the current log file.
  // Output : (none)
  void wipeFlash();

  // Quick state probes.
  bool     sdMounted()    const { return sdOk_; }
  bool     flashMounted() const { return flashOk_; }
  uint32_t failures()     const { return fails_; }
  String   sdPath()       const { return sdPath_; }
  String   flashPath()    const { return flashPath_; }

private:
#if USE_FLASH
  // Internal: walk the flash dir and stream files to Serial.
  void flashPrintRecursive_(const char* path);
  // Internal: walk the flash dir and delete every file/sub-folder.
  void flashDeleteRecursive_(const char* path);
  // Internal: write CSV_HEADER to `path` if the file is empty/new.
  void flashEnsureHeader_(const String& path);
#endif

#if USE_SD
  // Internal: write CSV_HEADER to `path` if the file is empty/new.
  void sdEnsureHeader_(const String& path);
#endif

  bool   sdOk_    = false;
  bool   flashOk_ = false;
  String sdPath_;
  String flashPath_;
  uint32_t fails_ = 0;
};
