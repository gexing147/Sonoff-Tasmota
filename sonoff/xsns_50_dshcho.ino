/*
  xsns_50_dshcho.ino - DS-HCHO sensor support for Sonoff-Tasmota

  Copyright (C) 2019  Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_DSHCHO

#define XSNS_50             50
#include "Arduino.h"
#include <TasmotaSerial.h>

TasmotaSerial *HchoSerial;

uint8_t hcho_type = 1;
uint8_t hcho_valid = 0;

struct hchodata {
  uint16_t start;
  uint8_t  len, id, unit, vh;
  uint16_t data;
  uint16_t checksum;
} hcho_data;

/*********************************************************************************************/

bool HchoReadData(void)
{
  if (! HchoSerial->available()) {
    return false;
  }

  if (HchoSerial->available() < 32) {
    return false;
  }

  uint8_t cmdBuffer[7] = {0x42,0x4d,0x01,0x00,0x00,0x00,0x90};
  uint8_t respBuffer[10] = {0};
  uint16_t sum = 0;

  HchoSerial->write(cmdBuffer, sizeof(cmdBuffer));

  while ((HchoSerial->peek() != 0x42) && HchoSerial->available()) {
    HchoSerial->read();
  }

  HchoSerial->readBytes(respBuffer, sizeof(respBuffer));
  HchoSerial->flush();  // Make room for another burst

  AddLogBuffer(LOG_LEVEL_DEBUG_MORE, cmdBuffer, sizeof(cmdBuffer));
  AddLogBuffer(LOG_LEVEL_DEBUG_MORE, respBuffer, sizeof(respBuffer));

  // get checksum ready
  for (uint8_t i = 0; i < 8; i++) {
    sum += respBuffer[i];
  }
  // The data comes in endian'd, this solves it so it works on all platforms
  uint16_t respBuffer_u16[5];
  for (uint8_t i = 0; i < 5; i++) {
    respBuffer_u16[i] = respBuffer[2 + i*2 + 1];
    respBuffer_u16[i] += (respBuffer[2 + i*2] << 8);
  }
  if (sum != respBuffer_u16[4]) {
    AddLog_P(LOG_LEVEL_DEBUG, PSTR("HCHO: " D_CHECKSUM_FAILURE));
    return false;
  }

  memcpy((void *)&hcho_data, (void *)respBuffer_u16, 10);

  switch(hcho_data.vh){
    case 1:
      hcho_data.data = hcho_data.data;
      break;
    case 2:
      hcho_data.data = hcho_data.data/10;
      break;
    case 3:
      hcho_data.data = hcho_data.data/100;
      break;
    case 4:
      hcho_data.data = hcho_data.data/1000;
      break;
  }            

  hcho_valid = 10;

  return true;
}

/*********************************************************************************************/

void HchoSecond(void)                 // Every second
{
  if (HchoReadData()) {
    hcho_valid = 10;
  } else {
    if (hcho_valid) {
      hcho_valid--;
    }
  }
}

/*********************************************************************************************/

void HchoInit(void)
{
  hcho_type = 0;
  if ((pin[GPIO_DS_HCHO_RX] < 99) && (pin[GPIO_DS_HCHO_TX] < 99)) {
    HchoSerial = new TasmotaSerial(pin[GPIO_DS_HCHO_RX], pin[GPIO_DS_HCHO_TX], 1);
    if (HchoSerial->begin(9600)) {
      if (HchoSerial->hardwareSerial()) { ClaimSerial(); }
      hcho_type = 1;
    }
  }
}

#ifdef USE_WEBSERVER
const char HTTP_DS_HCHO_SNS[] PROGMEM =
  "{s}DS-HCHO " D_FORMALDEHYDE_CONCENTRATION  "{m}%d " D_UNIT_PARTS_PER_DECILITER "{e}";      // {s} = <tr><th>, {m} = </th><td>, {e} = </td></tr>
#endif  // USE_WEBSERVER

void HchoShow(bool json)
{
  if (hcho_valid) {
    if (json) {
      ResponseAppend_P(PSTR(",\"DS-HCHO\":{\"LEN\":%d,\",\"ID\":%d,\"UNIT\":%d,\"VH\":%d,\"DATA\":%d,\"CHECKSUM\":%d}"),
        hcho_data.len, hcho_data.id, hcho_data.unit,
        hcho_data.vh, hcho_data.data, hcho_data.checksum);
#ifdef USE_DOMOTICZ
      if (0 == tele_period) {
        DomoticzSensor(DZ_CURRENT, hcho_data.data);  // PM10
      }
#endif  // USE_DOMOTICZ
#ifdef USE_WEBSERVER
    } else {
      WSContentSend_PD(HTTP_DS_HCHO_SNS,
//        pms_data.pm10_standard, pms_data.pm25_standard, pms_data.pm100_standard,
      hcho_data.data  );
#endif  // USE_WEBSERVER
    }
  }
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns50(uint8_t function)
{
  bool result = false;

  if (hcho_type) {
    switch (function) {
      case FUNC_INIT:
        HchoInit();
        break;
      case FUNC_EVERY_SECOND:
        HchoSecond();
        break;
      case FUNC_JSON_APPEND:
        HchoShow(1);
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        HchoShow(0);
        break;
#endif  // USE_WEBSERVER
    }
  }
  return result;
}

#endif  // USE_DSHCHO