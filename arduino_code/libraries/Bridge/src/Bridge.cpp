/*
  Copyright (c) 2013 Arduino LLC. All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "Bridge.h"

BridgeClass::BridgeClass(Stream &_stream) :
  index(0), stream(_stream), started(false), max_retries(0) {
  // Empty
}

void BridgeClass::begin() {
  if (started)
    return;
  started = true;

  // Wait for U-boot to finish startup
  do {
    dropAll();
    delay(1000);
  } while (stream.available() > 0);

  while (true) {
    // Bridge interrupt:
    // - Ask the bridge to close itself
    uint8_t quit_cmd[] = {'X', 'X', 'X', 'X', 'X'};
    max_retries = 1;
    transfer(quit_cmd, 5);

    // Bridge startup:
    // - If the bridge is not running starts it safely
    stream.print(CTRL_C);
    delay(250);
    stream.print(F("\n"));
    delay(250);
    stream.print(F("\n"));
    delay(500);
    // Wait for OpenWRT message
    // "Press enter to activate console"
    stream.print(F("run-bridge\n"));
    delay(500);
    dropAll();

    // Reset the brigde to check if it is running
    uint8_t cmd[] = {'X', 'X', '1', '0', '0'};
    uint8_t res[4];
    max_retries = 50;
    uint16_t l = transfer(cmd, 5, res, 4);
    if (l == TRANSFER_TIMEOUT) {
      // Bridge didn't start...
      // Maybe the board is starting-up?

      // Wait and retry
      delay(1000);
      continue;
    }
    if (res[0] != 0)
      while (true);

    // Detect bridge version
    if (l == 4) {
      bridgeVersion = (res[1]-'0')*100 + (res[2]-'0')*10 + (res[3]-'0');
    } else {
      // Bridge v1.0.0 didn't send any version info
      bridgeVersion = 100;
    }

    max_retries = 50;
    return;
  }
}

void BridgeClass::end() {

  while (true) {
    // Bridge interrupt:
    // - Ask the bridge to close itself
    uint8_t quit_cmd[] = {'X', 'X', 'X', 'X', 'X'};
    max_retries = 1;
    transfer(quit_cmd, 5);
    delay(100);
    stream.print(CTRL_C);
    delay(250);
    stream.print(F("cd \n"));
    //expect a shell
    bool done = false;
    delay(100);
    while (stream.available()) {
      char c = stream.read();
      if (c == '#') {
        done = true;
        break;
      }
    }
    if (done) {
      stream.print(F("reset\n"));
      break;
    }
  }
  delay(100);
  dropAll();
}

void BridgeClass::put(const char *key, const char *value) {
  // TODO: do it in a more efficient way
  String cmd = "D";
  uint8_t res[1];
  cmd += key;
  cmd += "\xFE";
  cmd += value;
  transfer((uint8_t*)cmd.c_str(), cmd.length(), res, 1);
}

unsigned int BridgeClass::get(const char *key, uint8_t *value, unsigned int maxlen) {
  uint8_t cmd[] = {'d'};
  unsigned int l = transfer(cmd, 1, (uint8_t *)key, strlen(key), value, maxlen);
  if (l < maxlen)
    value[l] = 0; // Zero-terminate string
  return l;
}

#if defined(ARDUINO_ARCH_AVR)
// AVR use an optimized implementation of CRC
#include <util/crc16.h>
#else
// Generic implementation for non-AVR architectures
uint16_t _crc_ccitt_update(uint16_t crc, uint8_t data)
{
  data ^= crc & 0xff;
  data ^= data << 4;
  return ((((uint16_t)data << 8) | ((crc >> 8) & 0xff)) ^
          (uint8_t)(data >> 4) ^
          ((uint16_t)data << 3));
}
#endif

void BridgeClass::crcUpdate(uint8_t c) {
  CRC = _crc_ccitt_update(CRC, c);
}

void BridgeClass::crcReset() {
  CRC = 0xFFFF;
}

void BridgeClass::crcWrite() {
  stream.write((char)(CRC >> 8));
  stream.write((char)(CRC & 0xFF));
}

bool BridgeClass::crcCheck(uint16_t _CRC) {
  return CRC == _CR                   @       @               @                     @                                     @                    @ (                                                       @     @                                                                        @    @                            0     `@@                 4a          @                                   @@ H @           @                                           @@      0           @                                                                  @                                           $              @  !                         @                   	         @                                                        @                                @                    @                                                    `  @             @ @@                                                    (   "              !                           @        @        D           @       D                                                @                         @ $     @                                            @     0                                                                      @)                          @   @          @                @@     "       A   `    @@                  @         @                       "   @      @                  A                     `                             @                                                     @                                       @                  `                                "       @                 @         (       	          @D            @        @                                    "   @                   @                                                                                             `              $                                       $      #   @       D                                                                                  !              !    @     @                          `  @@A             ("                           @                       p      @       D     @0        @    A                  4@     @   "                   @         $                                       	 @   @              @         @                                                                          @ @      `                        @   @                @                        P                              @ @                       E @   @                             @         (@@  @     @                              @       H                                              `H       @          @           @                   @             @                 @                                                            @                      B              @       ` !                                            " @                         @      @         @            B@   A  @                   @             @  @       @       @               D@                             @     @   @@       @     `@            @@       D                    @                                                @           @                                   @ @     @                      `                   @                          @                              