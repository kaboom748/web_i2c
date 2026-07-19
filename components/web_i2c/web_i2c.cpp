#include "web_i2c.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/alloc_helpers.h"

#include <cstring>
#include <cerrno>
#include <cstdio>

#ifdef USE_ARDUINO
#include <Arduino.h>
#endif
#ifdef USE_ESP32
#include <esp_system.h>
#endif
#ifdef USE_ESP_IDF
#include <driver/gpio.h>
#endif

#if defined(USE_ESP8266)
#define WI_MCU "ESP8266"
#elif defined(USE_ESP32_VARIANT_ESP32C3)
#define WI_MCU "ESP32-C3"
#elif defined(USE_ESP32_VARIANT_ESP32S3)
#define WI_MCU "ESP32-S3"
#elif defined(USE_ESP32_VARIANT_ESP32S2)
#define WI_MCU "ESP32-S2"
#elif defined(USE_ESP32)
#define WI_MCU "ESP32"
#else
#define WI_MCU "unknown"
#endif
#if defined(USE_ARDUINO)
#define WI_FW "Arduino"
#elif defined(USE_ESP_IDF)
#define WI_FW "ESP-IDF"
#else
#define WI_FW "unknown"
#endif

// tap best-effort: if the tx backlog exceeds this, drop the log frame (never
// grow unbounded). Smaller on the RAM-tight ESP8266.
#ifdef USE_ESP8266
#define WI_TAP_BACKLOG 2048
#else
#define WI_TAP_BACKLOG 8192
#endif

// dynamic throttle: the tap sends at most one log per (this x smoothed loop
// period). Fast loop = high rate; bogged loop = almost none. So the tap always
// leaves the web server most of whatever headroom the ESP currently has.
#define WI_TAP_LOOP_MULT 3

// boot protection: for the first WI_TAP_BOOT_GRACE ms, defer writes larger than
// WI_TAP_BOOT_MAX bytes (framebuffer pushes). A slow 50 kHz transfer of ~1 KB
// blocks the single-core ESP8266 long enough to starve the WiFi handshake at
// boot; small init/command writes still pass through so the device initializes.
#define WI_TAP_BOOT_MAX 64
#define WI_TAP_BOOT_GRACE 12000

// batch mode: flush the coalesced aggregation buffer this often (ms). One
// message per interval instead of one per transaction -- collapses the per-send
// cost so a chatty bus can't drown the ESP (WI_TAP_DATA_MAX lives in the .h).
#define WI_TAP_AGG_MS 200

// full-data only stores the bytes of SMALL COMMAND writes (decodable). Data-
// stream writes (control byte 0x40/0xC0 -- framebuffer pushes) are NEVER stored
// byte-by-byte regardless of size: unique pixel packets would fill the batch
// buffer and thrash the ESP8266 heap. Excluding them by control byte lets the
// size cap be generous for real command sequences without that risk.
#define WI_TAP_CMD_MAX 20

// framebuffer reconstruction: push the reconstructed screen at most this often.
// The cursor is driven by the SSD1306 addressing commands (not a time gap), so
// any write pattern -- fragmented, partial, manual -- reconstructs correctly.
#define WI_FB_MS 1000

namespace esphome {
namespace web_i2c {

static const char *const TAG = "web_i2c";

extern const char WEB_I2C_PAGE[];
extern const uint32_t WEB_I2C_PAGE_LEN;

namespace ws {

void Sha1::reset() {
  h_[0] = 0x67452301;
  h_[1] = 0xEFCDAB89;
  h_[2] = 0x98BADCFE;
  h_[3] = 0x10325476;
  h_[4] = 0xC3D2E1F0;
  len_ = 0;
  buf_len_ = 0;
}

void Sha1::update(const uint8_t *data, size_t len) {
  len_ += len;
  while (len > 0) {
    size_t take = 64 - buf_len_;
    if (take > len)
      take = len;
    memcpy(buf_ + buf_len_, data, take);
    buf_len_ += take;
    data += take;
    len -= take;
    if (buf_len_ == 64) {
      process_(buf_);
      buf_len_ = 0;
    }
  }
}

void Sha1::finish(uint8_t out[20]) {
  uint64_t bits = len_ * 8;
  uint8_t pad = 0x80;
  update(&pad, 1);
  uint8_t zero = 0;
  while (buf_len_ != 56)
    update(&zero, 1);
  uint8_t lenbuf[8];
  for (int i = 0; i < 8; i++)
    lenbuf[i] = (uint8_t) (bits >> (56 - i * 8));
  update(lenbuf, 8);
  for (int i = 0; i < 5; i++) {
    out[i * 4] = (uint8_t) (h_[i] >> 24);
    out[i * 4 + 1] = (uint8_t) (h_[i] >> 16);
    out[i * 4 + 2] = (uint8_t) (h_[i] >> 8);
    out[i * 4 + 3] = (uint8_t) (h_[i]);
  }
}

void Sha1::process_(const uint8_t *p) {
  uint32_t w[80];
  for (int i = 0; i < 16; i++)
    w[i] = (p[i * 4] << 24) | (p[i * 4 + 1] << 16) | (p[i * 4 + 2] << 8) | p[i * 4 + 3];
  for (int i = 16; i < 80; i++)
    w[i] = rol_(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4];
  for (int i = 0; i < 80; i++) {
    uint32_t f, k;
    if (i < 20) {
      f = (b & c) | (~b & d);
      k = 0x5A827999;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDC;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6;
    }
    uint32_t t = rol_(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = rol_(b, 30);
    b = a;
    a = t;
  }
  h_[0] += a;
  h_[1] += b;
  h_[2] += c;
  h_[3] += d;
  h_[4] += e;
}

std::string accept_key(const std::string &client_key) {
  static const char MAGIC[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  std::string concat = client_key + MAGIC;
  Sha1 s;
  s.update((const uint8_t *) concat.data(), concat.size());
  uint8_t digest[20];
  s.finish(digest);
  return base64_encode(digest, 20);
}

size_t decode_frame(const uint8_t *buf, size_t len, Frame &out) {
  if (len < 2)
    return 0;
  out.fin = (buf[0] & 0x80) != 0;
  out.opcode = buf[0] & 0x0F;
  bool masked = (buf[1] & 0x80) != 0;
  uint64_t plen = buf[1] & 0x7F;
  size_t pos = 2;
  if (plen == 126) {
    if (len < pos + 2)
      return 0;
    plen = ((uint64_t) buf[pos] << 8) | buf[pos + 1];
    pos += 2;
  } else if (plen == 127) {
    if (len < pos + 8)
      return 0;
    plen = 0;
    for (int i = 0; i < 8; i++)
      plen = (plen << 8) | buf[pos + i];
    pos += 8;
  }
  uint8_t mask[4] = {0, 0, 0, 0};
  if (masked) {
    if (len < pos + 4)
      return 0;
    for (int i = 0; i < 4; i++)
      mask[i] = buf[pos + i];
    pos += 4;
  }
  if (len < pos + plen)
    return 0;  // payload incomplet
  out.payload.resize(plen);
  for (uint64_t i = 0; i < plen; i++)
    out.payload[i] = buf[pos + i] ^ (masked ? mask[i & 3] : 0);
  return pos + plen;
}

void encode_frame(std::vector<uint8_t> &out, uint8_t opcode, const uint8_t *payload, size_t plen) {
  out.push_back(0x80 | (opcode & 0x0F));  // FIN + opcode
  if (plen < 126) {
    out.push_back((uint8_t) plen);
  } else if (plen <= 0xFFFF) {
    out.push_back(126);
    out.push_back((uint8_t) (plen >> 8));
    out.push_back((uint8_t) plen);
  } else {
    out.push_back(127);
    for (int i = 7; i >= 0; i--)
      out.push_back((uint8_t) (plen >> (i * 8)));
  }
  for (size_t i = 0; i < plen; i++)
    out.push_back(payload[i]);
}

}  // namespace ws

// ======================================================================
//  Component
// ======================================================================
void WebI2C::setup() {
  ESP_LOGCONFIG(TAG, "Web I2C terminal on port %u ready", this->port_);
#ifdef USE_ESP8266
  this->reset_str_ = ESP.getResetReason().c_str();
#elif defined(USE_ESP32)
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: this->reset_str_ = "Power-on"; break;
    case ESP_RST_SW: this->reset_str_ = "Software"; break;
    case ESP_RST_PANIC: this->reset_str_ = "Exception (panic)"; break;
    case ESP_RST_INT_WDT: this->reset_str_ = "Int watchdog"; break;
    case ESP_RST_TASK_WDT: this->reset_str_ = "Task watchdog"; break;
    case ESP_RST_BROWNOUT: this->reset_str_ = "Brownout"; break;
    case ESP_RST_DEEPSLEEP: this->reset_str_ = "Deep sleep"; break;
    case ESP_RST_EXT: this->reset_str_ = "External"; break;
    default: this->reset_str_ = "Unknown"; break;
  }
#endif
}

void WebI2C::dump_config() {
  ESP_LOGCONFIG(TAG, "Web I2C:");
  ESP_LOGCONFIG(TAG, "  Port: %u", this->port_);
}

void WebI2C::loop() {
  if (!this->server_started_)
    this->start_server_();
  if (this->server_ == nullptr)
    return;
  this->accept_client_();
  if (this->out_pos_ < this->out_.size())
    this->flush_tx_();
  if (this->stream_client_ != nullptr || this->pending_client_ != nullptr)
    this->read_client_();
  // guard delay: fresh millis() (anti-underflow, virtual_led_strip lesson)
  if (this->pending_client_ != nullptr && millis() - this->pending_since_ > 3000) {
    this->pending_client_->close();
    this->pending_client_ = nullptr;
    this->request_len_ = 0;
    this->nl_ = 0;
  }
  // measure the main-loop period (smoothed) -- the ESP's "headroom" meter. The
  // web server runs in this loop, so a slow loop means it is starving; the tap
  // reads this to throttle itself dynamically (see tap_log_).
  uint32_t nowu = micros();
  if (this->last_loop_us_ != 0) {
    uint32_t dt = nowu - this->last_loop_us_;
    if (dt > 100000)
      dt = 100000;  // cap absurd gaps (a blocking lambda) at 100 ms
    this->loop_ema_us_ = this->loop_ema_us_ ? (this->loop_ema_us_ * 7 + dt) / 8 : dt;
  }
  this->last_loop_us_ = nowu;
  // transactions-per-second (Observed rate), recomputed once per second
  if (millis() - this->last_rate_ms_ >= 1000) {
    uint32_t dtm = millis() - this->last_rate_ms_;
    this->obs_rate_ = (this->observed_ - this->last_observed_) * 1000 / dtm;
    this->last_observed_ = this->observed_;
    this->batch_rate_ = (this->batch_msgs_ - this->last_batch_msgs_) * 1000 / dtm;
    this->last_batch_msgs_ = this->batch_msgs_;
    this->last_rate_ms_ = millis();
  }
  // track heap low-water mark + push live info to the browser every ~2 s.
  // NEVER while serving the page: appending a WS frame to out_ mid-serve would
  // inject bytes into the HTTP body and truncate the page.
  uint32_t h = this->free_heap_();
  if (h != 0 && (this->min_heap_ == 0 || h < this->min_heap_))
    this->min_heap_ = h;
  if (this->stream_client_ != nullptr && !this->serving_page_ &&
      millis() - this->last_info_ > 2000) {
    this->last_info_ = millis();
    this->send_info_();
  }
  // batch mode: flush the coalesced buffer as one message every WI_TAP_AGG_MS
  if (this->tap_batch_ && millis() - this->last_agg_ms_ >= WI_TAP_AGG_MS) {
    this->last_agg_ms_ = millis();
    this->flush_tap_agg_();
  }
  // screen mirror: push the reconstructed framebuffer when it changed
  if (this->tap_fb_ && this->tap_fb_dirty_ && this->stream_client_ != nullptr && !this->serving_page_ &&
      millis() - this->last_fb_ms_ >= WI_FB_MS && this->out_.size() - this->out_pos_ <= WI_TAP_BACKLOG) {
    this->send_fb_();
    this->last_fb_ms_ = millis();
    this->tap_fb_dirty_ = false;
  }
}

// ======================================================================
//  WebSocket server (proven skeleton from virtual_uart)
// ======================================================================
void WebI2C::start_server_() {
  this->server_started_ = true;
  this->server_ = socket::socket_ip_loop_monitored(SOCK_STREAM, 0);
  if (this->server_ == nullptr) {
    ESP_LOGE(TAG, "socket failed");
    this->mark_failed();
    return;
  }
  if (this->server_->setblocking(false) != 0) {
    this->mark_failed();
    return;
  }
  int enable = 1;
  this->server_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  struct sockaddr_storage sa {};
  socklen_t sl = socket::set_sockaddr_any(reinterpret_cast<struct sockaddr *>(&sa), sizeof(sa), this->port_);
  if (this->server_->bind(reinterpret_cast<struct sockaddr *>(&sa), sl) != 0 || this->server_->listen(4) != 0) {
    ESP_LOGE(TAG, "bind/listen failed on port %u", this->port_);
    this->server_ = nullptr;
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "listening on port %u", this->port_);
}

void WebI2C::accept_client_() {
  if (this->server_ == nullptr || this->pending_client_ != nullptr || this->stream_client_ != nullptr)
    return;
  // accept_loop_monitored (not accept): registering in the select() loop
  // is required on ESP32 (virtual_led_strip lesson)
  auto client = this->server_->accept_loop_monitored(nullptr, nullptr);
  if (client == nullptr)
    return;
  client->setblocking(false);
  this->pending_client_ = std::move(client);
  this->request_len_ = 0;
  this->nl_ = 0;
  this->pending_since_ = millis();
}

void WebI2C::read_client_() {
  // HTTP handshake
  if (this->pending_client_ != nullptr) {
    uint8_t b[128];
    for (;;) {
      const ssize_t len = this->pending_client_->read(b, sizeof(b));
      if (len == 0) {
        this->pending_client_->close();
        this->pending_client_ = nullptr;
        return;
      }
      if (len < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
          return;
        this->pending_client_->close();
        this->pending_client_ = nullptr;
        return;
      }
      this->pending_since_ = millis();
      for (ssize_t i = 0; i < len; i++) {
        const char c = (char) b[i];
        if (this->request_len_ + 1 < sizeof(this->request_) && c != '\r')
          this->request_[this->request_len_++] = c;
        if (c == '\n') {
          if (++this->nl_ >= 2) {
            this->request_[this->request_len_] = '\0';
            this->do_handshake_(this->request_, this->request_len_);
            return;
          }
        } else if (c != '\r') {
          this->nl_ = 0;
        }
      }
    }
  }
  // WebSocket actif: draine + decoupe les frames
  if (this->stream_client_ != nullptr) {
    uint8_t b[512];
    for (;;) {
      const ssize_t len = this->stream_client_->read(b, sizeof(b));
      if (len == 0) {
        this->drop_client_("peer closed");
        return;
      }
      if (len < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
          break;
        this->drop_client_("read error");
        return;
      }
      this->ws_accum_.insert(this->ws_accum_.end(), b, b + len);
      // Anti-OOM bound: no legitimate command approaches this size. A client
      // sending a giant frame (declared huge payload) is dropped instead of
      // growing the buffer until the heap is exhausted.
      if (this->ws_accum_.size() > 8192) {
        this->drop_client_("frame too large");
        return;
      }
    }
    for (;;) {
      ws::Frame fr;
      size_t used = ws::decode_frame(this->ws_accum_.data(), this->ws_accum_.size(), fr);
      if (used == 0)
        break;
      this->ws_accum_.erase(this->ws_accum_.begin(), this->ws_accum_.begin() + used);
      this->handle_ws_frame_(fr);
      if (this->stream_client_ == nullptr)
        break;
    }
  }
}

bool WebI2C::do_handshake_(const char *request, size_t len) {
  const char *kh = nullptr;
  static const char NEEDLE[] = "sec-websocket-key:";
  for (const char *p = request; *p; p++) {
    size_t j = 0;
    while (NEEDLE[j] && p[j] &&
           (p[j] == NEEDLE[j] || (p[j] >= 'A' && p[j] <= 'Z' && (p[j] + 32) == NEEDLE[j])))
      j++;
    if (NEEDLE[j] == '\0') {
      kh = p + j;
      break;
    }
  }
  if (kh == nullptr) {
    // GET normal -> servir la page
    if (strncmp(request, "GET / ", 6) == 0 || strncmp(request, "GET /?", 6) == 0 ||
        strncmp(request, "GET / HTTP", 10) == 0) {
      this->page_len_ = WEB_I2C_PAGE_LEN;
      this->page_pos_ = 0;
      std::string headers = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
                            "Cache-Control: no-store\r\nConnection: close\r\nContent-Length: " +
                            std::to_string(this->page_len_) + "\r\n\r\n";
      // only the headers go into out_; the body is streamed in chunks
      // from flash in flush_tx_ (no allocation of the whole page).
      this->out_.assign(headers.begin(), headers.end());
      this->out_pos_ = 0;
      this->stream_client_ = std::move(this->pending_client_);
      this->pending_client_ = nullptr;
      this->serving_page_ = true;
      this->flush_tx_();
    } else {
      static const char NF[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
      this->pending_client_->write(NF, sizeof(NF) - 1);
      this->pending_client_->close();
      this->pending_client_ = nullptr;
    }
    return false;
  }
  while (*kh == ' ')
    kh++;
  std::string key;
  while (*kh && *kh != '\r' && *kh != '\n')
    key.push_back(*kh++);
  std::string accept = ws::accept_key(key);
  std::string resp = "HTTP/1.1 101 Switching Protocols\r\n"
                     "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                     "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
  this->stream_client_ = std::move(this->pending_client_);
  this->pending_client_ = nullptr;
  this->serving_page_ = false;
  this->ws_accum_.clear();
  this->out_.insert(this->out_.end(), resp.begin(), resp.end());
  this->out_pos_ = 0;
  this->flush_tx_();
  ESP_LOGD(TAG, "websocket client connected");
  this->ws_send_text_("{\"t\":\"hello\",\"msg\":\"Web I2C ready\"}");
  this->last_info_ = 0;  // first info push happens from loop() (shallow stack)
  return true;
}

void WebI2C::handle_ws_frame_(const ws::Frame &frame) {
  switch (frame.opcode) {
    case ws::OP_CLOSE:
      this->drop_client_("close frame");
      break;
    case ws::OP_PING:
      this->ws_send_(ws::OP_PONG, frame.payload.data(), frame.payload.size());
      break;
    case ws::OP_TEXT:
    case ws::OP_BIN:
      if (!frame.payload.empty()) {
        std::string line(reinterpret_cast<const char *>(frame.payload.data()), frame.payload.size());
        this->handle_command_(line);
      }
      break;
    default:
      break;
  }
}

void WebI2C::ws_send_text_(const std::string &text) {
  this->ws_send_(ws::OP_TEXT, reinterpret_cast<const uint8_t *>(text.data()), text.size());
}

// ws_send_ / flush_tx_ / drop_client_ : faithful copy of virtual_uart
void WebI2C::ws_send_(uint8_t opcode, const uint8_t *payload, size_t len) {
  if (this->stream_client_ == nullptr)
    return;
  ws::encode_frame(this->out_, opcode, payload, len);
  this->flush_tx_();
}

bool WebI2C::flush_tx_() {
  auto *client = this->stream_client_ ? this->stream_client_.get() : nullptr;
  if (client == nullptr) {
    this->out_.clear();
    this->out_pos_ = 0;
    this->serving_page_ = false;
    return false;
  }
  for (;;) {
    // pousser ce qui reste dans out_
    while (this->out_pos_ < this->out_.size()) {
      const ssize_t w = client->write(this->out_.data() + this->out_pos_, this->out_.size() - this->out_pos_);
      if (w > 0) {
        this->out_pos_ += (size_t) w;
        continue;
      }
      if (w == 0 || errno == EAGAIN || errno == EWOULDBLOCK)
        return true;  // buffer plein: on reprendra a la prochaine passe (pas de close, lecon virtual_led_strip)
      this->drop_client_("write error");
      return false;
    }
    this->out_.clear();
    this->out_pos_ = 0;
    // out_ empty: if serving the page, reload the next chunk from
    // flash (out_ never exceeds ~512 B, so no big ESP8266 malloc)
    if (this->serving_page_ && this->page_pos_ < this->page_len_) {
      size_t chunk = this->page_len_ - this->page_pos_;
      if (chunk > 512)
        chunk = 512;
      this->out_.resize(chunk);
      progmem_memcpy(this->out_.data(), WEB_I2C_PAGE + this->page_pos_, chunk);
      this->page_pos_ += chunk;
      this->out_pos_ = 0;
      continue;  // reboucle pour envoyer ce morceau
    }
    // nothing left to send: close if it was the page
    if (this->serving_page_) {
      this->stream_client_->close();
      this->stream_client_ = nullptr;
      this->serving_page_ = false;
    }
    return true;
  }
}

void WebI2C::drop_client_(const char *why) {
  ESP_LOGD(TAG, "ws client dropped: %s", why);
  if (this->stream_client_ != nullptr) {
    this->stream_client_->close();
    this->stream_client_ = nullptr;
  }
  this->out_.clear();
  this->out_pos_ = 0;
  this->ws_accum_.clear();
  this->serving_page_ = false;
}

// ======================================================================
//  I2C command parser. Text format, a keyword + hex arguments:
//    SCAN                     -> scans the bus, lists present addresses
//    W <addr> <b0> <b1> ...   -> writes raw bytes (ACK/NACK)
//    R <addr> <len>           -> reads <len> raw bytes
//    WR <addr> <reg> <b0> ... -> writes to a register (addr, then reg+data)
//    RR <addr> <reg> <len>    -> reads <len> bytes from a register (write_readv)
//  All values in hexadecimal. JSON responses for the page.
// ======================================================================
const char *WebI2C::err_str_(uint8_t code) {
  switch (code) {
    case i2c::ERROR_OK: return "OK";
    case i2c::ERROR_INVALID_ARGUMENT: return "BAD_ARG";
    case i2c::ERROR_NOT_ACKNOWLEDGED: return "NACK";
    case i2c::ERROR_TIMEOUT: return "TIMEOUT";
    case i2c::ERROR_NOT_INITIALIZED: return "NOT_INIT";
    case i2c::ERROR_TOO_LARGE: return "TOO_LARGE";
    case i2c::ERROR_CRC: return "CRC";
    case i2c::ERROR_UNKNOWN: return "BUS_ERROR";
    default: return "UNKNOWN";
  }
}

// advance p to the next non-space token; false if end of string
static bool skip_to_token(const char *&p) {
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
    p++;
  return *p != '\0';
}

// read ONE hex byte at position p ("3C" / "0x3C" / "3c"), WITHOUT allocating;
// advance p past the token. false if the token is not a valid hex byte.
static bool read_hex(const char *&p, uint8_t &out) {
  const char *s = p;
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    s += 2;
  unsigned v = 0;
  int nd = 0;
  while (*s && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') {
    char c = *s;
    unsigned d;
    if (c >= '0' && c <= '9') d = c - '0';
    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
    else return false;
    v = v * 16 + d;
    if (v > 0xFF) return false;
    nd++;
    s++;
  }
  if (nd == 0)
    return false;
  out = (uint8_t) v;
  p = s;
  return true;
}

// helper: bytes -> hex string "3C AF 01"
static std::string hex_join(const uint8_t *data, size_t len) {
  std::string s;
  char buf[4];
  for (size_t i = 0; i < len; i++) {
    snprintf(buf, sizeof(buf), "%02X", data[i]);
    if (i) s += " ";
    s += buf;
  }
  return s;
}

void WebI2C::handle_command_(const std::string &line) {
  if (this->bus_ == nullptr) {
    this->ws_send_text_("{\"t\":\"err\",\"msg\":\"no I2C bus\"}");
    return;
  }
  // DIRECT pointer parser, NO token vector: on a loaded ESP8266,
  // tokenizing a 64-byte command created ~67 std::string (~2 KB per block)
  // -> OOM. Here we read hex bytes directly from the line (stack only).
  const char *p = line.c_str();
  if (!skip_to_token(p))
    return;
  // keyword in uppercase in a small stack buffer
  char kw[8];
  int kl = 0;
  while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
    if (kl < 7) {
      char c = *p;
      if (c >= 'a' && c <= 'z') c -= 32;
      kw[kl++] = c;
    }
    p++;
  }
  kw[kl] = '\0';

  if (strcmp(kw, "SCAN") == 0) {
    this->cmd_scan_();
    return;
  }
  if (strcmp(kw, "RECOVER") == 0) {
    this->recover_bus_();
    return;
  }
  if (strcmp(kw, "TAP") == 0) {
    // TAP <FULL|SUMMARY|OFF|BATCH|LIVE> [addr] -- parsed by first letter
    char mode = 'F';
    if (skip_to_token(p)) {
      mode = (*p >= 'a' && *p <= 'z') ? (char) (*p - 32) : *p;
      while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
        p++;
    }
    this->flush_tap_agg_();  // send anything pending before switching modes
    if (mode == 'B') {
      this->tap_batch_ = true;  // BATCH: coalesce + flush periodically
    } else if (mode == 'L') {
      this->tap_batch_ = false;  // LIVE: one message per transaction
    } else if (mode == 'T') {
      this->tap_takeover_ = true;  // TAKEOVER: frontend owns the bus
    } else if (mode == 'R') {
      this->tap_takeover_ = false;  // RELAY: normal, devices reach the bus
    } else if (mode == 'M') {
      // MIRROR <1|0>: framebuffer reconstruction on/off
      this->tap_fb_ = !(skip_to_token(p) && *p == '0');
      if (this->tap_fb_) {  // start from clean addressing defaults (horizontal, full)
        this->fb_mode_ = 0;
        this->fb_col_ = 0;
        this->fb_page_ = 0;
        this->fb_col_start_ = 0;
        this->fb_col_end_ = 127;
        this->fb_page_start_ = 0;
        this->fb_page_end_ = 7;
        this->fb_cmd_wait_ = 0;
        this->fb_skip_ = 0;
        this->fb_param_i_ = 0;
      }
    } else if (mode == 'O') {
      this->tap_off_ = true;
    } else {
      this->tap_off_ = false;
      this->tap_full_ = (mode != 'S');  // FULL unless explicitly SUMMARY
      uint8_t a;
      this->tap_filter_ = (skip_to_token(p) && read_hex(p, a) && a <= 0x7F) ? (int) a : -1;
    }
    this->send_info_();  // reflect the new mode in the Info panel
    return;
  }

  // all other commands have an address
  uint8_t addr;
  if (!skip_to_token(p) || !read_hex(p, addr) || addr > 0x7F) {
    this->ws_send_text_("{\"t\":\"err\",\"msg\":\"invalid address\"}");
    return;
  }

  if (strcmp(kw, "W") == 0) {
    std::vector<uint8_t> data;
    data.reserve(64);  // un bloc framebuffer tient en une allocation
    while (skip_to_token(p)) {
      uint8_t b;
      if (!read_hex(p, b)) {
        this->ws_send_text_("{\"t\":\"err\",\"msg\":\"invalid byte\"}");
        return;
      }
      data.push_back(b);
    }
    this->cmd_write_(addr, data);
  } else if (strcmp(kw, "R") == 0) {
    uint8_t len;
    if (!skip_to_token(p) || !read_hex(p, len)) {
      this->ws_send_text_("{\"t\":\"err\",\"msg\":\"invalid length\"}");
      return;
    }
    this->cmd_read_(addr, len);
  } else if (strcmp(kw, "WR") == 0) {
    uint8_t reg;
    if (!skip_to_token(p) || !read_hex(p, reg)) {
      this->ws_send_text_("{\"t\":\"err\",\"msg\":\"invalid register\"}");
      return;
    }
    std::vector<uint8_t> data;
    data.reserve(32);
    while (skip_to_token(p)) {
      uint8_t b;
      if (!read_hex(p, b)) {
        this->ws_send_text_("{\"t\":\"err\",\"msg\":\"invalid byte\"}");
        return;
      }
      data.push_back(b);
    }
    this->cmd_write_reg_(addr, reg, data);
  } else if (strcmp(kw, "RR") == 0) {
    uint8_t reg, len;
    if (!skip_to_token(p) || !read_hex(p, reg) || !skip_to_token(p) || !read_hex(p, len)) {
      this->ws_send_text_("{\"t\":\"err\",\"msg\":\"invalid value\"}");
      return;
    }
    this->cmd_read_reg_(addr, reg, len);
  } else {
    this->ws_send_text_("{\"t\":\"err\",\"msg\":\"unknown command\"}");
  }
}

void WebI2C::cmd_scan_() {
  // scan 0x08..0x77 (valid 7-bit range) by probing presence via write(len=0)
  std::string found;
  int count = 0;
  for (uint8_t a = 0x08; a <= 0x77; a++) {
    auto err = this->bus_->write(a, nullptr, 0);
    if (err == i2c::ERROR_OK) {
      char buf[8];
      snprintf(buf, sizeof(buf), "\"%02X\"", a);
      if (count) found += ",";
      found += buf;
      count++;
    }
  }
  std::string msg = "{\"t\":\"scan\",\"found\":[" + found + "],\"count\":" + std::to_string(count) + "}";
  this->ws_send_text_(msg);
  ESP_LOGD(TAG, "scan: %d device(s)", count);
}

void WebI2C::cmd_write_(uint8_t addr, const std::vector<uint8_t> &data) {
  auto err = this->bus_->write(addr, data.data(), data.size());
  // Feed manual writes to the mirror too: both the positioning commands
  // (20/21/22 the composer sends) and the pixel data. Device traffic reaches
  // the mirror via write_readv; manual commands bypass it (they call bus_
  // directly), so without this the mirror shows nothing in take-over mode (where
  // device traffic is blocked and only manual commands reach the bus).
  if (this->tap_fb_ && data.size() >= 2)
    this->tap_fb_feed_(data.data(), data.size());
  // response built in a stack buffer (no std::string) to further reduce
  // allocations further on the framebuffer hot path.
  char msg[96];
  snprintf(msg, sizeof(msg), "{\"t\":\"write\",\"addr\":\"%02X\",\"n\":%u,\"res\":\"%s\"}",
           addr, (unsigned) data.size(), this->err_str_(err));
  this->ws_send_(ws::OP_TEXT, (const uint8_t *) msg, strlen(msg));
  // Do not log every block of a framebuffer send: on a loaded ESP8266, these
  // DEBUG logs accumulate (sent to the connected API) and end up in OOM.
  if (data.size() <= 8)
    ESP_LOGD(TAG, "W 0x%02X (%u bytes): %s", addr, (unsigned) data.size(), this->err_str_(err));
}

void WebI2C::cmd_read_(uint8_t addr, size_t len) {
  std::vector<uint8_t> buf(len);
  auto err = this->bus_->read(addr, buf.data(), len);
  std::string data_hex = (err == i2c::ERROR_OK) ? hex_join(buf.data(), len) : "";
  std::string msg = "{\"t\":\"read\",\"addr\":\"" + hex_join(&addr, 1) + "\",\"res\":\"" +
                    this->err_str_(err) + "\",\"data\":\"" + data_hex + "\"}";
  this->ws_send_text_(msg);
  ESP_LOGD(TAG, "R 0x%02X (%u bytes): %s [%s]", addr, (unsigned) len, this->err_str_(err), data_hex.c_str());
}

void WebI2C::cmd_write_reg_(uint8_t addr, uint8_t reg, const std::vector<uint8_t> &data) {
  // write [reg, data...] in one transaction
  std::vector<uint8_t> full;
  full.push_back(reg);
  full.insert(full.end(), data.begin(), data.end());
  auto err = this->bus_->write(addr, full.data(), full.size());
  std::string msg = "{\"t\":\"writereg\",\"addr\":\"" + hex_join(&addr, 1) + "\",\"reg\":\"" +
                    hex_join(&reg, 1) + "\",\"res\":\"" + this->err_str_(err) + "\"}";
  this->ws_send_text_(msg);
  ESP_LOGD(TAG, "WR 0x%02X reg 0x%02X: %s", addr, reg, this->err_str_(err));
}

void WebI2C::cmd_read_reg_(uint8_t addr, uint8_t reg, size_t len) {
  // write_readv: write the register, restart, read len bytes
  std::vector<uint8_t> buf(len);
  auto err = this->bus_->write_readv(addr, &reg, 1, buf.data(), len);
  std::string data_hex = (err == i2c::ERROR_OK) ? hex_join(buf.data(), len) : "";
  std::string msg = "{\"t\":\"readreg\",\"addr\":\"" + hex_join(&addr, 1) + "\",\"reg\":\"" +
                    hex_join(&reg, 1) + "\",\"res\":\"" + this->err_str_(err) + "\",\"data\":\"" + data_hex + "\"}";
  this->ws_send_text_(msg);
  ESP_LOGD(TAG, "RR 0x%02X reg 0x%02X (%u bytes): %s [%s]", addr, reg, (unsigned) len, this->err_str_(err), data_hex.c_str());
}

uint32_t WebI2C::free_heap_() {
#ifdef USE_ESP8266
  return ESP.getFreeHeap();
#elif defined(USE_ESP32)
  return esp_get_free_heap_size();
#else
  return 0;
#endif
}

// idle level of a bus line; a snapshot read between transactions is safe and
// does not change the pin mode owned by the I2C peripheral.
int WebI2C::pin_level_(int pin) {
  if (pin < 0)
    return -1;
#if defined(USE_ARDUINO)
  return digitalRead(pin);
#elif defined(USE_ESP_IDF)
  return gpio_get_level((gpio_num_t) pin);
#else
  return -1;
#endif
}

// push all backend info (static config + live heap/levels/uptime) to the page
void WebI2C::send_info_() {
  if (this->stream_client_ == nullptr || this->serving_page_)
    return;
  uint32_t heap = this->free_heap_();
  int sda_lvl = this->pin_level_(this->sda_pin_);
  int scl_lvl = this->pin_level_(this->scl_pin_);
  // built on the heap (std::string), like the other responses -- no large
  // stack buffer, which would overflow the small ESP8266 stack when this is
  // reached from a deep call path.
  std::string j = "{\"t\":\"info\",\"freq\":" + std::to_string(this->bus_freq_) +
                  ",\"sda\":" + std::to_string((int) this->sda_pin_) +
                  ",\"scl\":" + std::to_string((int) this->scl_pin_) +
                  ",\"scan\":" + (this->bus_scan_ ? "true" : "false") +
                  ",\"sdapu\":" + std::to_string((int) this->sda_pullup_) +
                  ",\"sclpu\":" + std::to_string((int) this->scl_pullup_) +
                  ",\"sdalvl\":" + std::to_string(sda_lvl) +
                  ",\"scllvl\":" + std::to_string(scl_lvl) +
                  ",\"heap\":" + std::to_string(heap) +
                  ",\"minheap\":" + std::to_string(this->min_heap_) +
                  ",\"uptime\":" + std::to_string(millis() / 1000) +
                  ",\"port\":" + std::to_string(this->port_) +
                  ",\"tapcount\":" + std::to_string(this->tap_count_) +
                  ",\"observed\":" + std::to_string(this->observed_) +
                  ",\"dropped\":" + std::to_string(this->dropped_) +
                  ",\"obsrate\":" + std::to_string(this->obs_rate_) +
                  ",\"loophz\":" + std::to_string(this->loop_ema_us_ ? (1000000u / this->loop_ema_us_) : 0u) +
                  ",\"bootheld\":" + std::to_string(this->boot_deferred_) +
                  ",\"tapfull\":" + (this->tap_full_ ? "true" : "false") +
                  ",\"tapoff\":" + (this->tap_off_ ? "true" : "false") +
                  ",\"tapfilter\":" + std::to_string(this->tap_filter_) +
                  ",\"tapbatch\":" + (this->tap_batch_ ? "true" : "false") +
                  ",\"taptakeover\":" + (this->tap_takeover_ ? "true" : "false") +
                  ",\"tapfb\":" + (this->tap_fb_ ? "true" : "false") +
                  ",\"batchrate\":" + std::to_string(this->batch_rate_) +
                  ",\"tapskip\":" +
                  ((this->last_skip_ != 0 && millis() - this->last_skip_ < 1500) ? "true" : "false") +
                  ",\"mcu\":\"" WI_MCU "\",\"fw\":\"" WI_FW "\",\"reset\":\"" + this->reset_str_ + "\"}";
  this->ws_send_text_(j);
}

// ---- i2c::I2CBus transparent decorator ----
// write_readv is the sole pure virtual in this ESPHome version; every device
// read/write/register-read arrives here (directly, or via the legacy read()/
// write() shims that call it). Relay to the real bus FIRST (sacred: never
// blocked by the monitor), capture the result, then best-effort log it. The
// direction is inferred from the counts: write-only, read-only, or write-read.
i2c::ErrorCode WebI2C::write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count,
                                   uint8_t *read_buffer, size_t read_count) {
  if (this->bus_ == nullptr || this->bus_ == static_cast<i2c::I2CBus *>(this))
    return i2c::ERROR_NOT_INITIALIZED;
  this->observed_++;  // count every device transaction (the load meter)
  char dir;
  size_t n;
  const uint8_t *dbuf;
  size_t dlen;
  if (write_count > 0 && read_count > 0) {
    dir = 'X';  // write-then-read (register read); log the register bytes
    n = write_count + read_count;
    dbuf = write_buffer;
    dlen = write_count;
  } else if (read_count > 0) {
    dir = 'R';
    n = read_count;
    dbuf = read_buffer;  // the bytes read back (valid after the relay below)
    dlen = read_count;
  } else {
    dir = 'W';
    n = write_count;
    dbuf = write_buffer;  // the bytes written
    dlen = write_count;
  }
  // TAKEOVER: the frontend owns the bus -- device traffic is NOT relayed to the
  // hardware, so manual terminal commands (which call bus_ directly) have it to
  // themselves. Return success so the device doesn't error; nothing reaches the
  // bus and nothing is logged.
  if (this->tap_takeover_)
    return i2c::ERROR_OK;
  // BOOT PROTECTION: during the first WI_TAP_BOOT_GRACE ms, skip heavy writes
  // (framebuffer pushes) so a slow transfer can't block the loop and starve the
  // WiFi handshake. Small writes (init/commands) pass so the device initializes.
  if (read_count == 0 && write_count > WI_TAP_BOOT_MAX && millis() < WI_TAP_BOOT_GRACE) {
    this->boot_deferred_++;
    return i2c::ERROR_OK;  // pretend success -- nothing is corrupted, just skipped
  }
  i2c::ErrorCode err = this->bus_->write_readv(address, write_buffer, write_count, read_buffer, read_count);
  if (this->tap_fb_ && read_count == 0 && write_count >= 2)
    this->tap_fb_feed_(write_buffer, write_count);  // commands + pixels -> mirror
  this->tap_log_(address, dir, n, err, dbuf, dlen);
  return err;
}

// classify a transaction for the log: a read, a bulk DATA stream (SSD1306-style
// 0x40/0xC0 control byte -> pixels to GDDRAM), or a command/register write.
static const char *wi_tap_kind(char dir, const uint8_t *data, size_t dlen) {
  if (dir == 'R' || dir == 'X')
    return "read";
  if (data != nullptr && dlen > 0 && (data[0] == 0x40 || data[0] == 0xC0))
    return "data";
  return "cmd";
}

// best-effort log of one relayed transaction. Always counts (cheap); only
// builds/sends a frame when a client is watching, the page isn't being served,
// and the tx backlog is small. Otherwise it drops and marks "skipping".
void WebI2C::tap_log_(uint8_t addr, char dir, size_t n, i2c::ErrorCode err, const uint8_t *data,
                      size_t data_len) {
  if (this->stream_client_ == nullptr || this->serving_page_ || this->tap_off_)
    return;
  if (this->tap_filter_ >= 0 && (int) addr != this->tap_filter_)
    return;  // address filter: only log the chosen address
  if (this->tap_batch_) {
    this->tap_aggregate_(addr, dir, n, err, data, data_len);
    return;  // coalesced; flushed as one message from loop()
  }
  // dynamic CPU throttle: send at most one log per (K x smoothed loop-period).
  // Fast loop (headroom) -> high rate; bogged loop -> almost none, so the tap
  // always yields CPU to the web server. Caps frame-building to a steady rate
  // instead of the bursts the out_ gate alone allowed. Checked first + cheap.
  uint32_t nowu = micros();
  if (this->loop_ema_us_ != 0 && (nowu - this->last_tap_us_) < this->loop_ema_us_ * WI_TAP_LOOP_MULT) {
    this->dropped_++;
    this->last_skip_ = millis();
    return;
  }
  if (this->out_.size() - this->out_pos_ > WI_TAP_BACKLOG) {
    this->dropped_++;
    this->last_skip_ = millis();
    return;
  }
  const char *d = (dir == 'R') ? "R" : (dir == 'X') ? "RR" : "W";
  std::string j = "{\"t\":\"tap\",\"us\":" + std::to_string(nowu) + ",\"kind\":\"" + wi_tap_kind(dir, data, data_len) + "\",\"dir\":\"" + std::string(d) + "\",\"addr\":\"" + hex_join(&addr, 1) +
                  "\",\"n\":" + std::to_string((unsigned) n) + ",\"res\":\"" + this->err_str_(err) + "\"";
  if (this->tap_full_ && data != nullptr && data_len > 0 && data_len <= WI_TAP_CMD_MAX &&
      data[0] != 0x40 && data[0] != 0xC0) {
    j += ",\"data\":\"" + hex_join(data, data_len) + "\"";  // command bytes (not data streams)
  }
  j += "}";
  this->ws_send_text_(j);
  this->last_tap_us_ = nowu;
}

// Coalesce one transaction into the fixed buffer: bump an identical entry's
// count, or claim a new slot. No allocation; when full, drop distinct patterns
// until the next flush (bounded). Cost per transaction: a short linear scan.
void WebI2C::tap_aggregate_(uint8_t addr, char dir, size_t n, i2c::ErrorCode err, const uint8_t *data,
                            size_t data_len) {
  uint8_t dl = 0;  // store bytes only for small COMMAND writes -- see WI_TAP_CMD_MAX
  if (this->tap_full_ && data != nullptr && data_len > 0 && data_len <= WI_TAP_CMD_MAX &&
      data[0] != 0x40 && data[0] != 0xC0)
    dl = (uint8_t) data_len;
  uint16_t nn = (uint16_t) (n > 0xFFFF ? 0xFFFF : n);
  for (int i = 0; i < this->tap_agg_used_; i++) {
    TapEntry &e = this->tap_agg_[i];
    if (e.addr == addr && e.dir == dir && e.res == err && e.n == nn && e.data_len == dl &&
        (dl == 0 || memcmp(e.data, data, dl) == 0)) {
      e.count++;
      return;
    }
  }
  if (this->tap_agg_used_ >= WI_TAP_AGG_SLOTS) {
    this->dropped_++;  // buffer full of distinct patterns; drop until next flush
    return;
  }
  TapEntry &e = this->tap_agg_[this->tap_agg_used_++];
  e.addr = addr;
  e.dir = dir;
  e.res = err;
  e.n = nn;
  e.data_len = dl;
  if (dl > 0)
    memcpy(e.data, data, dl);
  e.count = 1;
  e.us = micros();
  e.kind = (dir == 'R' || dir == 'X') ? 2 : (data != nullptr && data_len > 0 && (data[0] == 0x40 || data[0] == 0xC0)) ? 1 : 0;
}

// Send the whole buffer as ONE message, then reset. Skips (keeps buffering) if
// no client, mid page-serve, or the link is backed up.
void WebI2C::flush_tap_agg_() {
  if (this->tap_agg_used_ == 0)
    return;
  if (this->stream_client_ == nullptr || this->serving_page_)
    return;
  if (this->out_.size() - this->out_pos_ > WI_TAP_BACKLOG)
    return;
  std::string j = "{\"t\":\"tapbatch\",\"txns\":[";
  for (int i = 0; i < this->tap_agg_used_; i++) {
    TapEntry &e = this->tap_agg_[i];
    const char *d = (e.dir == 'R') ? "R" : (e.dir == 'X') ? "RR" : "W";
    const char *ek = (e.kind == 2) ? "read" : (e.kind == 1) ? "data" : "cmd";
    if (i > 0)
      j += ",";
    j += "{\"us\":" + std::to_string(e.us) + ",\"kind\":\"" + ek + "\",\"dir\":\"" + std::string(d) + "\",\"addr\":\"" + hex_join(&e.addr, 1) +
         "\",\"n\":" + std::to_string(e.n) + ",\"res\":\"" + this->err_str_(e.res) +
         "\",\"count\":" + std::to_string(e.count);
    if (e.data_len > 0)
      j += ",\"data\":\"" + hex_join(e.data, e.data_len) + "\"";
    j += "}";
  }
  j += "]}";
  this->ws_send_text_(j);
  this->batch_msgs_++;
  this->tap_agg_used_ = 0;
}

// ---- I2C bus recovery (bit-bang, portable) ----
// portable single-pin primitives; Arduino uses pinMode/digitalWrite/Read, IDF
// uses the gpio driver. Levels are plain 0/1 to avoid HIGH/LOW on IDF.
static void wi_pin_drive(int pin, int level) {
#if defined(USE_ARDUINO)
  pinMode(pin, OUTPUT);
  digitalWrite(pin, level);
#elif defined(USE_ESP_IDF)
  gpio_set_direction((gpio_num_t) pin, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t) pin, level);
#else
  (void) pin;
  (void) level;
#endif
}
static void wi_pin_release(int pin) {  // input + pull-up (floats high unless held)
#if defined(USE_ARDUINO)
  pinMode(pin, INPUT_PULLUP);
#elif defined(USE_ESP_IDF)
  gpio_set_direction((gpio_num_t) pin, GPIO_MODE_INPUT);
  gpio_set_pull_mode((gpio_num_t) pin, GPIO_PULLUP_ONLY);
#else
  (void) pin;
#endif
}
static int wi_pin_get(int pin) {
#if defined(USE_ARDUINO)
  return digitalRead(pin);
#elif defined(USE_ESP_IDF)
  return gpio_get_level((gpio_num_t) pin);
#else
  (void) pin;
  return 1;
#endif
}

// On-demand only (RECOVER command): frees a slave that is holding SDA low by
// clocking SCL up to 9 times, then issuing a STOP. Never runs during traffic
// (single-threaded, triggered from a WS command). Leaves both lines released;
// on ESP8266 the next real transaction reconfigures Wire and re-owns the pins.
void WebI2C::recover_bus_() {
  int sda = this->sda_pin_, scl = this->scl_pin_;
  if (sda < 0 || scl < 0) {
    this->ws_send_text_("{\"t\":\"recover\",\"res\":\"no_pins\",\"sda\":-1,\"pulses\":0}");
    return;
  }
  wi_pin_release(sda);   // let the pull-up / stuck slave own SDA
  wi_pin_drive(scl, 1);  // SCL idle high
  delayMicroseconds(10);
  int pulses = 0;
  for (int i = 0; i < 9; i++) {
    if (wi_pin_get(sda))
      break;  // slave released SDA
    wi_pin_drive(scl, 0);
    delayMicroseconds(10);
    wi_pin_drive(scl, 1);
    delayMicroseconds(10);
    pulses++;
  }
  // STOP: with SCL high, take SDA low then release it (low->high transition)
  wi_pin_drive(sda, 0);
  delayMicroseconds(10);
  wi_pin_drive(scl, 1);
  delayMicroseconds(10);
  wi_pin_release(sda);
  delayMicroseconds(10);
  int freed = wi_pin_get(sda);
  wi_pin_release(scl);  // release both; next transaction re-owns the pins
  std::string j = "{\"t\":\"recover\",\"res\":\"" + std::string(freed ? "released" : "still_low") +
                  "\",\"sda\":" + std::to_string(freed) + ",\"pulses\":" + std::to_string(pulses) + "}";
  this->ws_send_text_(j);
  this->send_info_();  // refresh the Info panel's line levels immediately
}

// Parameter count for the SSD1306 commands that take arguments (so their param
// bytes aren't mistaken for positioning opcodes). 20/21/22 are handled directly.
static uint8_t fb_cmd_params(uint8_t c) {
  switch (c) {
    case 0x81:  // contrast
    case 0x8D:  // charge pump
    case 0xA8:  // multiplex
    case 0xD3:  // display offset
    case 0xD5:  // clock divide
    case 0xD9:  // pre-charge
    case 0xDA:  // com pins
    case 0xDB:  // vcom detect
      return 1;
    case 0xA3:  // vertical scroll area
      return 2;
    case 0x29:  // vertical + right scroll
    case 0x2A:  // vertical + left scroll
      return 5;
    case 0x26:  // right scroll
    case 0x27:  // left scroll
      return 6;
    default:
      return 0;
  }
}

// Feed one relayed/manual write to the mirror: command stream (control 0x00/0x80)
// advances the addressing parser; data stream (0x40/0xC0) writes pixels.
void WebI2C::tap_fb_feed_(const uint8_t *data, size_t len) {
  if (len < 2)
    return;
  uint8_t ctrl = data[0];
  if (ctrl == 0x00 || ctrl == 0x80) {
    for (size_t i = 1; i < len; i++)
      this->fb_cmd_byte_(data[i]);
  } else if (ctrl == 0x40 || ctrl == 0xC0) {
    for (size_t i = 1; i < len; i++)
      this->fb_data_byte_(data[i]);
  }
}

void WebI2C::fb_cmd_byte_(uint8_t b) {
  if (this->fb_skip_ > 0) {  // consuming a non-positioning command's parameters
    this->fb_skip_--;
    return;
  }
  if (this->fb_cmd_wait_ != 0) {  // collecting a positioning command's parameters
    this->fb_params_[this->fb_param_i_++] = b;
    uint8_t need = (this->fb_cmd_wait_ == 0x20) ? 1 : 2;
    if (this->fb_param_i_ >= need) {
      if (this->fb_cmd_wait_ == 0x20) {
        this->fb_mode_ = this->fb_params_[0] & 0x03;
      } else if (this->fb_cmd_wait_ == 0x21) {
        this->fb_col_start_ = this->fb_params_[0];
        this->fb_col_end_ = this->fb_params_[1];
        this->fb_col_ = this->fb_col_start_;  // 0x21 resets the column pointer
      } else {                                // 0x22
        this->fb_page_start_ = this->fb_params_[0] & 0x07;
        this->fb_page_end_ = this->fb_params_[1] & 0x07;
        this->fb_page_ = this->fb_page_start_;  // 0x22 resets the page pointer
      }
      this->fb_cmd_wait_ = 0;
    }
    return;
  }
  if (b == 0x20 || b == 0x21 || b == 0x22) {
    this->fb_cmd_wait_ = b;
    this->fb_param_i_ = 0;
  } else if (b >= 0xB0 && b <= 0xB7) {
    this->fb_page_ = b & 0x07;  // page mode: set page
  } else if (b <= 0x0F) {
    this->fb_col_ = (this->fb_col_ & 0xF0) | (b & 0x0F);  // lower column nibble
  } else if (b >= 0x10 && b <= 0x1F) {
    this->fb_col_ = (this->fb_col_ & 0x0F) | ((uint8_t) (b & 0x0F) << 4);  // upper nibble
  } else {
    this->fb_skip_ = fb_cmd_params(b);  // skip any other command's parameters
  }
}

void WebI2C::fb_data_byte_(uint8_t d) {
  uint16_t idx = (uint16_t) this->fb_page_ * 128 + this->fb_col_;
  if (idx < sizeof(this->tap_fb_buf_))
    this->tap_fb_buf_[idx] = d;
  if (this->fb_mode_ == 1) {  // vertical: page advances first
    if (this->fb_page_ >= this->fb_page_end_) {
      this->fb_page_ = this->fb_page_start_;
      this->fb_col_ = (this->fb_col_ >= this->fb_col_end_) ? this->fb_col_start_ : this->fb_col_ + 1;
    } else {
      this->fb_page_++;
    }
  } else if (this->fb_mode_ == 0) {  // horizontal: column advances first
    if (this->fb_col_ >= this->fb_col_end_) {
      this->fb_col_ = this->fb_col_start_;
      this->fb_page_ = (this->fb_page_ >= this->fb_page_end_) ? this->fb_page_start_ : this->fb_page_ + 1;
    } else {
      this->fb_col_++;
    }
  } else {  // page mode: column advances within the page, wraps at 127
    this->fb_col_ = (this->fb_col_ >= 127) ? 0 : this->fb_col_ + 1;
  }
  this->tap_fb_dirty_ = true;
}

void WebI2C::send_fb_() {
  // Change detection: skip identical frames. A static image at 10 fps would
  // otherwise rebuild + resend 2 KB every interval; that repeated allocation
  // fragments the heap until a big WiFi/Noise buffer alloc fails -> OOM crash.
  static uint32_t last_hash = 0xFFFFFFFFu;
  uint32_t h = 2166136261u;  // FNV-1a over the reconstructed buffer
  for (size_t i = 0; i < sizeof(this->tap_fb_buf_); i++) {
    h ^= this->tap_fb_buf_[i];
    h *= 16777619u;
  }
  if (h == last_hash)
    return;  // nothing changed since the last send -> don't allocate or send
  last_hash = h;
  // Reuse a persistent string (keeps its capacity across calls) instead of
  // allocating ~2 KB on every send -- that churn was the fragmentation source.
  static const char HEXD[] = "0123456789ABCDEF";  // NB: HEX is an Arduino macro
  static std::string j;
  if (j.capacity() < sizeof(this->tap_fb_buf_) * 2 + 24)
    j.reserve(sizeof(this->tap_fb_buf_) * 2 + 24);
  j.clear();
  j += "{\"t\":\"fb\",\"data\":\"";
  for (size_t i = 0; i < sizeof(this->tap_fb_buf_); i++) {
    j += HEXD[this->tap_fb_buf_[i] >> 4];
    j += HEXD[this->tap_fb_buf_[i] & 0x0F];
  }
  j += "\"}";
  this->ws_send_text_(j);
}

}  // namespace web_i2c
}  // namespace esphome
