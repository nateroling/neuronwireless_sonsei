// Host-side harness: compiles the real Qukeys::processQueue() (extracted from
// QukeysDygma.cpp) against minimal stubs, and plays timed key sequences
// through it one millisecond "cycle" at a time, the way beforeReportingState()
// drives it on the keyboard.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

struct Key {
  int v;
  bool operator==(const Key &o) const { return v == o.v; }
};
using KeyAddr = int;
static const Key Key_NoKey{0};

struct RuntimeT {
  uint32_t now = 0;
  template <class T, class U> bool hasTimeExpired(T start, U ttl) {
    T cur = (T)now;
    T el = cur - start;
    return el > ttl;
  }
} Runtime;
void kbdpwr_sleep_postpone() {}

// Keys: addr 1 = qukey (tap 'q' = 100, hold = ctrl = 1000). Other addrs map to
// plain printable keys (addr * 10). Values >= 1000 are modifiers.
bool isModifierKey(Key k) { return k.v >= 1000; }
static std::string name(KeyAddr a, Key k) {
  if (k.v == 1000) return "CTRL";
  if (k.v == 100) return "q";
  if (k.v == 0) return std::string(1, char('a' + a - 2));
  return std::string(1, char('a' + k.v / 10 - 2));
}

struct Queue {
  struct E { KeyAddr a; bool press; uint16_t t; };
  std::vector<E> e;
  bool isEmpty() const { return e.empty(); }
  uint8_t length() const { return e.size(); }
  KeyAddr addr(uint8_t i) const { return e[i].a; }
  bool isPress(uint8_t i) const { return e[i].press; }
  bool isRelease(uint8_t i) const { return !e[i].press; }
  uint16_t timestamp(uint8_t i) const { return e[i].t; }
  bool isFull() const { return e.size() >= 8; }
  void shift() { e.erase(e.begin()); }
  void append(KeyAddr a, bool p) { e.push_back({a, p, (uint16_t)Runtime.now}); }
};

struct Cfg {
  uint16_t hold_timeout;
  uint8_t overlap_threshold;
  uint8_t minimum_hold_time;
  uint8_t minimum_prior_interval;
};

struct Qukeys {
  Queue event_queue_;
  bool active_ = true;
  bool hold_on_other_key_press_ = true;
  const Cfg *p_Qukeys_config;
  uint16_t prior_keypress_timestamp_ = 256;
  struct { Key primary_key{-1}; Key alternate_key{-1}; } queue_head_;
  std::vector<std::string> out;

  bool isQukey(KeyAddr k) {
    if (k == 1) { queue_head_.primary_key = {100}; queue_head_.alternate_key = {1000}; return true; }
    queue_head_.primary_key = {k * 10};
    queue_head_.alternate_key = {-1};
    return false;
  }
  void flushEvent(Key k) {
    auto ev = event_queue_.e[0];
    if (ev.press && k.v > 0 && k.v < 1000) prior_keypress_timestamp_ = ev.t;
    char buf[64];
    if (ev.press)
      snprintf(buf, sizeof buf, "%s@%u", name(ev.a, k).c_str(), (unsigned)Runtime.now);
    else
      snprintf(buf, sizeof buf, "^%s", ev.a == 1 ? "Q" : name(ev.a, Key_NoKey).c_str());
    out.push_back(buf);
    event_queue_.shift();
  }
  bool processQueue();
  bool releaseDelayed(uint16_t overlap_start, uint16_t overlap_end) const;
  bool qukeyHeldThroughMinimumHoldTime() const;

  // Same shape as Qukeys::beforeReportingState() minus housekeeping.
  void cycle() {
    if (Runtime.hasTimeExpired(prior_keypress_timestamp_, p_Qukeys_config->minimum_prior_interval))
      prior_keypress_timestamp_ = Runtime.now - (p_Qukeys_config->minimum_prior_interval + 1);
    if (processQueue()) return;
    if (Runtime.hasTimeExpired(event_queue_.timestamp(0), p_Qukeys_config->hold_timeout)) {
      Key k = isModifierKey(queue_head_.primary_key) ? queue_head_.primary_key : queue_head_.alternate_key;
      flushEvent(k);
    }
  }
};

#include FNS

struct Ev { uint32_t t; KeyAddr a; bool press; };

static std::string run(const std::vector<Ev> &evs, bool hold_on_press) {
  static const Cfg cfg{250, 80, 50, 75};
  Qukeys q;
  q.p_Qukeys_config = &cfg;
  q.hold_on_other_key_press_ = hold_on_press;
  Runtime.now = 1000;  // start away from 0 like a running keyboard
  size_t i = 0;
  for (uint32_t ms = 0; ms < 800; ++ms, ++Runtime.now) {
    while (i < evs.size() && evs[i].t == ms) {
      q.event_queue_.append(evs[i].a, evs[i].press);
      if (q.event_queue_.isFull()) q.processQueue();
      ++i;
    }
    q.cycle();
  }
  std::string s;
  for (auto &o : q.out) {
    // report times relative to t=0
    auto at = o.find('@');
    if (at != std::string::npos) {
      unsigned t = std::stoul(o.substr(at + 1));
      s += o.substr(0, at) + "@" + std::to_string(t - 1000) + " ";
    } else {
      s += o + " ";
    }
  }
  return s;
}

// Settings: hold timeout 250, overlap threshold 80, minimum hold 50, minimum
// prior interval 75 (Dygma defaults). Q is a qukey: tap = q, hold = CTRL.
// Output: "<key>@<ms>" is a press sent to the host at that time, "^x" a release.
// "off" expectations are stock Qukeys behavior (verified identical to the
// unpatched file).
int main() {
  const KeyAddr Q = 1, A = 2, B = 3, C = 4;
  struct Case { const char *name; std::vector<Ev> evs; const char *on; const char *off; };
  std::vector<Case> cases = {
    {"A nested chord", {{0,Q,1},{100,B,1},{150,B,0},{200,Q,0}},
     "CTRL@100 b@101 ^b ^Q ", "CTRL@150 b@151 ^b ^Q "},
    {"B roll off the qukey", {{0,Q,1},{100,B,1},{120,Q,0},{180,B,0}},
     "CTRL@100 b@101 ^Q ^b ", "q@126 b@127 ^Q ^b "},
    {"C fast roll < min hold", {{0,Q,1},{20,B,1},{40,Q,0},{70,B,0}},
     "q@46 b@47 ^Q ^b ", "q@46 b@47 ^Q ^b "},
    {"D early press, still held", {{0,Q,1},{20,B,1},{200,B,0},{300,Q,0}},
     "CTRL@51 b@52 ^b ^Q ", "CTRL@200 b@201 ^b ^Q "},
    {"E solo tap", {{0,Q,1},{100,Q,0}},
     "q@100 ^Q ", "q@100 ^Q "},
    {"F solo hold", {{0,Q,1},{400,Q,0}},
     "CTRL@251 ^Q ", "CTRL@251 ^Q "},
    {"G right after typing",
     {{0,A,1},{30,A,0},{50,Q,1},{120,B,1},{150,B,0},{200,Q,0}},
     "a@0 ^a q@50 b@120 ^b ^Q ", "a@0 ^a q@50 b@120 ^b ^Q "},
    {"H burst in one cycle",
     {{0,A,1},{10,A,0},{50,Q,1},{50,B,1},{50,C,1},{90,B,0},{95,C,0},{120,Q,0}},
     "a@0 ^a q@50 b@51 c@52 ^b ^c ^Q ", "a@0 ^a q@50 b@51 c@52 ^b ^c ^Q "},
  };
  int failures = 0;
  for (auto &c : cases) {
    for (int flag = 1; flag >= 0; --flag) {
      std::string got = run(c.evs, flag);
      const char *want = flag ? c.on : c.off;
      bool ok = got == want;
      failures += !ok;
      printf("%s  %-26s [%s] %s\n", ok ? "PASS" : "FAIL", c.name, flag ? "on " : "off", got.c_str());
      if (!ok) printf("      expected: %s\n", want);
    }
  }
  printf("%d failure(s)\n", failures);
  return failures ? 1 : 0;
}
