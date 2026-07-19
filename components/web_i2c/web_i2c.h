#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/components/socket/socket.h"

#include <memory>
#include <vector>
#include <string>
#include <cstdint>

// tap full-data / aggregation sizing. Bytes stored per entry (also the live
// full-data cap). Aggregation buffer is a FIXED array (no dynamic allocation
// per transaction) -- small on the RAM-tight ESP8266, larger on the ESP32.
#define WI_TAP_DATA_MAX 32
#if defined(USE_ESP8266)
#define WI_TAP_AGG_SLOTS 16
#else
#define WI_TAP_AGG_SLOTS 48
#endif

namespace esphome {
namespace web_i2c {

namespace ws {

class Sha1 {
 public:
  Sha1() { reset(); }
  void reset();
  void update(const uint8_t *data, size_t len);
  void finish(uint8_t out[20]);

 private:
  static uint32_t rol_(uint32_t v, int b) { return (v << b) | (v >> (32 - b)); }
  void process_(const uint8_t *p);
  uint32_t h_[5];
  uint64_t len_;
  uint8_t buf_[64];
  size_t buf_len_;
};

// Compute Sec-WebSocket-Accept from the client key.
std::string accept_key(const std::string &client_key);

enum Opcode : uint8_t {
  OP_CONT = 0x0,
  OP_TEXT = 0x1,
  OP_BIN = 0x2,
  OP_CLOSE = 0x8,
  OP_PING = 0x9,
  OP_PONG = 0xA,
};

// A decoded frame.
struct Frame {
  bool fin;
  uint8_t opcode;
  std::vector<uint8_t> payload;
};

// Decode ONE frame from buf. Returns the bytes consumed, or 0 if the
// frame is incomplete (wait for more data).
size_t decode_frame(const uint8_t *buf, size_t len, Frame &out);

// Encode a server->client frame (never masked).
void encode_frame(std::vector<uint8_t> &out, uint8_t opcode, const uint8_t *payload, size_t plen);

}  // namespace ws

// ======================================================================
//  I2C debug terminal. Serves a web page; receives text commands
//  (SCAN/READ/WRITE/READREG/WRITEREG) over WebSocket; runs the real
//  I2C transactions on the ESP's bus; returns the results.
// ======================================================================
class WebI2C : public Component, public i2c::I2CBus {
 public:
  void set_port(uint16_t port) { this->port_ = port; }
  void set_i2c_bus(i2c::I2CBus *bus) { this->bus_ = bus; }
  void set_tap_count(int n) { this->tap_count_ = n; }
  void set_bus_info(uint32_t freq, int sda, int scl, bool scan, int sdapu, int sclpu) {
    this->bus_freq_ = freq;
    this->sda_pin_ = (int16_t) sda;
    this->scl_pin_ = (int16_t) scl;
    this->bus_scan_ = scan;
    this->sda_pullup_ = (int8_t) sdapu;
    this->scl_pullup_ = (int8_t) sclpu;
  }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  // --- i2c::I2CBus transparent decorator. In this ESPHome version write_readv
  // is the only pure virtual; the legacy read()/write() funnel through it, so
  // overriding it alone intercepts every transaction. Relay to the real bus,
  // then observe. Only invoked when another component points its i2c_id at us;
  // our own manual commands call bus_ directly and are not tapped here.
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count,
                             uint8_t *read_buffer, size_t read_count) override;

 protected:
  // --- server / websocket (same proven skeleton as virtual_uart) ---
  void start_server_();
  void accept_client_();
  void read_client_();
  void handle_ws_frame_(const ws::Frame &frame);
  void handle_command_(const std::string &line);  // decode and run an I2C command
  bool do_handshake_(const char *request, size_t len);
  void ws_send_text_(const std::string &text);    // send a response to the browser
  void ws_send_(uint8_t opcode, const uint8_t *payload, size_t len);
  bool flush_tx_();
  void drop_client_(const char *why);

  // --- I2C operations ---
  void cmd_scan_();
  void cmd_write_(uint8_t addr, const std::vector<uint8_t> &data);
  void cmd_read_(uint8_t addr, size_t len);
  void cmd_write_reg_(uint8_t addr, uint8_t reg, const std::vector<uint8_t> &data);
  void cmd_read_reg_(uint8_t addr, uint8_t reg, size_t len);
  const char *err_str_(uint8_t code);

  // --- backend info surfaced to the frontend ---
  void send_info_();       // push the {"t":"info",...} JSON to the browser
  uint32_t free_heap_();   // portable free-heap read (ESP8266/ESP32)
  int pin_level_(int pin); // idle level of a GPIO (-1 if unavailable)
  // best-effort tap log of one relayed transaction (dir 'W'/'R'/'X'); data/
  // data_len carry the bytes for full-data mode (ignored in summary mode)
  void tap_log_(uint8_t addr, char dir, size_t n, i2c::ErrorCode err, const uint8_t *data, size_t data_len);
  void tap_aggregate_(uint8_t addr, char dir, size_t n, i2c::ErrorCode err, const uint8_t *data,
                      size_t data_len);
  void flush_tap_agg_();   // send the coalesced batch and reset the buffer
  void recover_bus_();     // bit-bang 9 SCL pulses + STOP to free a stuck slave
  void tap_fb_feed_(const uint8_t *data, size_t len);  // route a write to the mirror
  void fb_cmd_byte_(uint8_t b);   // advance the SSD1306 command/addressing parser
  void fb_data_byte_(uint8_t d);  // write one pixel byte at the cursor, then advance
  void send_fb_();         // push the reconstructed framebuffer to the browser

  i2c::I2CBus *bus_{nullptr};
  uint16_t port_{0};

  // bus config injected at build time from the i2c: config, plus live info
  uint32_t bus_freq_{0};
  int16_t sda_pin_{-1};
  int16_t scl_pin_{-1};
  bool bus_scan_{false};
  int8_t sda_pullup_{-1};
  int8_t scl_pullup_{-1};
  uint32_t min_heap_{0};
  uint32_t last_info_{0};
  std::string reset_str_{"n/a"};

  // transparent tap (bus monitor) state
  int tap_count_{0};        // components wired to us at build time
  uint32_t observed_{0};    // transactions relayed through the tap (since boot)
  uint32_t dropped_{0};     // log frames skipped because the link was backed up
  uint32_t last_skip_{0};   // millis() of the last drop (for sticky "skipping")
  // dynamic throttle + load meters
  uint32_t last_loop_us_{0};   // micros() at the previous loop() call
  uint32_t loop_ema_us_{0};    // smoothed main-loop period (the headroom meter)
  uint32_t last_tap_us_{0};    // micros() of the last tap log actually sent
  uint32_t obs_rate_{0};       // observed transactions per second
  uint32_t last_observed_{0};  // observed_ snapshot for the rate window
  uint32_t last_rate_ms_{0};   // millis() at the last rate computation
  uint32_t boot_deferred_{0};  // heavy writes held back during the boot window
  // framebuffer reconstruction: a FIXED 1 KB buffer (one 128x64 screen). Pixel
  // data writes overwrite it by position -- bounded by construction, unlike the
  // aggregation buffer, so this is safe even on the ESP8266.
  bool tap_fb_{false};          // screen-mirror mode on
  uint8_t tap_fb_buf_[1024];    // reconstructed GDDRAM (page-major)
  bool tap_fb_dirty_{false};    // buffer changed since the last send
  uint32_t last_fb_ms_{0};      // millis() of the last framebuffer send
  // SSD1306 addressing state -- tracks the real column/page pointer from the
  // driver's commands (21/22/20, page-mode B0-B7 + column nibbles) so any write
  // pattern reconstructs correctly, fragmented or not. No time-gap heuristic.
  uint8_t fb_mode_{0};          // 0 horizontal, 1 vertical, 2 page
  uint8_t fb_col_{0}, fb_page_{0};                  // current pointer
  uint8_t fb_col_start_{0}, fb_col_end_{127};       // column window (cmd 21)
  uint8_t fb_page_start_{0}, fb_page_end_{7};       // page window (cmd 22)
  uint8_t fb_cmd_wait_{0}, fb_param_i_{0}, fb_skip_{0};  // command-parser state
  uint8_t fb_params_[2]{0, 0};
  bool tap_full_{false};       // full-data logging (emit the bytes) vs summary
  bool tap_off_{false};        // logging disabled entirely (still relays+counts)
  int tap_filter_{-1};         // only log this address (-1 = all addresses)
  bool tap_batch_{false};      // aggregate + dedupe, flush periodically (vs live)
  bool tap_takeover_{false};   // frontend owns the bus: device traffic not relayed
  // fixed-size aggregation buffer -- no allocation per transaction
  struct TapEntry {
    uint8_t addr;
    char dir;
    i2c::ErrorCode res;
    uint16_t n;                     // total byte count of the transaction
    uint8_t data_len;               // bytes stored (0 in summary mode)
    uint8_t data[WI_TAP_DATA_MAX];  // captured bytes (full mode)
    uint32_t count;                 // identical transactions coalesced into this
    uint32_t us;                    // micros() of the first occurrence in this slot
    uint8_t kind;                   // 0=cmd, 1=data (pixel stream), 2=read
  };
  TapEntry tap_agg_[WI_TAP_AGG_SLOTS];
  int tap_agg_used_{0};        // slots currently in use
  uint32_t last_agg_ms_{0};    // millis() of the last batch flush
  uint32_t batch_msgs_{0};     // batch messages sent (cumulative, for the rate)
  uint32_t batch_rate_{0};     // batch messages per second
  uint32_t last_batch_msgs_{0};

  std::unique_ptr<socket::ListenSocket> server_;
  std::unique_ptr<socket::Socket> pending_client_;
  std::unique_ptr<socket::Socket> stream_client_;
  bool server_started_{false};
  bool serving_page_{false};

  char request_[1024]{};
  size_t request_len_{0};
  uint8_t nl_{0};
  uint32_t pending_since_{0};

  std::vector<uint8_t> ws_accum_;
  std::vector<uint8_t> out_;
  size_t out_pos_{0};

  // Serve the page in chunks from flash (no full copy in RAM: a ~20 KB
  // page would fail malloc and reboot the ESP8266).
  size_t page_pos_{0};
  size_t page_len_{0};
};

}  // namespace web_i2c
}  // namespace esphome
