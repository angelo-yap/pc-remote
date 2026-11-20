#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <MD5Builder.h>
#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include <esp_rom_crc.h>
#include "secrets.h"
#include "plug.h"

static const uint16_t PLUG_PORT = 6668;
static const uint32_t CMD_CONTROL = 0x07;
static const uint32_t CMD_DP_QUERY = 0x0A;
static const uint32_t PLUG_TIMEOUT_MS = 3000;

static uint32_t seqNo = 1;

//write 32-bit number as 4 big-endian bytes
static void putU32(uint8_t* p, uint32_t v){
  p[0] = v >> 24;
  p[1] = v >> 16;
  p[2] = v >> 8;
  p[3] = v;
}

//read 4 big-endian bytes into number (parsing purposes)
static uint32_t getU32(const uint8_t* p){
  return ((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | (uint32_t)p[3]);
}

static size_t buildFrame(uint32_t cmd, const uint8_t* payload, size_t len, uint8_t* out){
  putU32(out, 0x000055AA);
  putU32(out + 4, seqNo++);
  putU32(out + 8, cmd);
  putU32(out + 12, len + 8);
  memcpy(out + 16, payload, len);

  uint32_t crc = esp_rom_crc32_le(0, out, 16 + len);

  putU32(out + 16 + len, crc);
  putU32(out + 20 + len, 0x0000AA55);
  return 24 + len;
}

static String basePayload(bool forQuery){
  JsonDocument doc;
  String t = String((unsigned long)time(nullptr));

  if(forQuery) doc["gwId"]=PLUG_ID;

  doc["devId"] = PLUG_ID;
  doc["uid"] = PLUG_ID;
  doc["t"] = t;

  if(!forQuery) doc["dps"].to<JsonObject>();
    
  String out;
  serializeJson(doc, out);
  return out;
}

static String encryptPayload(const String& json){
  //Padding
  //Data length must be a multiple of 16.
  //PKCS7 rule: add N bytes that each have the value N.
  //e.g. 110 bytes -> 110 % 16 = 14 -> pad = 2 -> add 0x02 0x02 -> 112 bytes
  size_t len = json.length();
  size_t pad = 16 - (len % 16);
  size_t padded = len + pad;

  uint8_t plain[512]; //input buffer: Json + padding
  uint8_t enc[512]; //output buffer: Encrypted bytes

  if(padded > sizeof(plain)) return "";

  memcpy(plain, json.c_str(), len);
  memset(plain + len, pad, pad);

  // AES-128-ECB
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, (const uint8_t*)PLUG_KEY, 128);

  for(size_t i = 0; i < padded; i += 16){
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, plain + i, enc + i);
  }

  mbedtls_aes_free(&aes);

  // Base64
  uint8_t b64[700];
  size_t b64len = 0;

  if(mbedtls_base64_encode(b64, sizeof(b64), &b64len, enc, padded) != 0) return "";

  String b64str = String((const char*)b64,b64len);

  // Signature
  MD5Builder md5;
  md5.begin();
  md5.add(String("data=") + b64str + "||lpv=3.1||" + PLUG_KEY);
  md5.calculate();
  String sig = md5.toString().substring(8,24);

  return String("3.1") + sig + b64str;
}

static bool readExact(WiFiClient& c, uint8_t* buf, size_t n){
  size_t got = 0;
  unsigned long start = millis();

  while(got < n){
    if(millis() - start >= PLUG_TIMEOUT_MS) return false;

    if(!c.connected() && c.available() == 0) return false;

    int r = c.read(buf + got, n - got);
    
    if (r > 0) got += r;
    else delay(5);
  }
  return true;
}

static String sendCommand(uint32_t cmd, const String& payload){
  WiFiClient c;

  //Connect
  if(!c.connect(PLUG_IP, PLUG_PORT, PLUG_TIMEOUT_MS)){
    Serial.println("Plug: connect failed");
    return "";
  }

  //build & send frame
  uint8_t frame[512];
  //does not fit
  if(payload.length() + 24 > sizeof(frame)){
    c.stop();
    return "";
  }

  size_t size = buildFrame(cmd, (const uint8_t*)payload.c_str(), payload.length(), frame);
  c.write(frame, size);

  // Read reply header
  uint8_t header[16];
  if(!readExact(c, header, 16)){
    Serial.println("Plug: no reply");
    c.stop();
    return "";
  }

  //check if real tuya frame
  if(getU32(header) != 0x000055AA){
    c.stop();
    return "";
  }

  // Read the rest
  uint32_t restLen = getU32(header + 12);
  uint8_t rest[1024];

  //sanity check
  if(restLen < 8 || restLen > sizeof(rest)){
    c.stop();
    return "";
  }
  
  //read
  if(!readExact(c, rest, restLen)){
    Serial.println("Plug: reply cut off");
    c.stop();
    return "";
  }

  c.stop();

  // Unpack payload
  size_t payloadLen = restLen - 8;
  
  if(payloadLen < 4 || getU32(rest) != 0){
    Serial.println("Plug: error reply");
    return "";
  }

  const char* body = (const char*)rest + 4;
  size_t bodyLen = payloadLen - 4;

  if(bodyLen == 0){
    return "{}";
  }else if(body[0] == '{'){
    return String(body, bodyLen);
  }else{
    return "{}";
  }
}

bool plugSetPower(bool on){
  JsonDocument doc;
  if(deserializeJson(doc, basePayload(false))) return false;
  doc["dps"]["1"] = on;

  String json;
  serializeJson(doc, json);
  return sendCommand(CMD_CONTROL, encryptPayload(json)) != "";
}

bool plugOffFor(int seconds){
  JsonDocument doc;
  if(deserializeJson(doc, basePayload(false))) return false;
  doc["dps"]["1"] = false;
  doc["dps"]["11"] = seconds;

  String json;
  serializeJson(doc, json);
  if(sendCommand(CMD_CONTROL, encryptPayload(json)) == "") return false;

  // confirm off before reporting success
  delay(500);
  bool on;
  return plugGetPower(on) && !on;
}

bool plugGetPower(bool& on){
  on = false;
  String reply = sendCommand(CMD_DP_QUERY, basePayload(true));
  if(reply == "") return false;

  JsonDocument doc;
  //parse error
  if(deserializeJson(doc, reply)) return false;

  //no switch value in reply
  if(!doc["dps"]["1"].is<bool>()) return false;

  on = doc["dps"]["1"];
  return true;
}
