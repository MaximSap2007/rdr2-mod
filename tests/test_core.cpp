// Unit tests for the rewind core. No game needed: run with build.sh.
#include <cmath>
#include <cstdio>
#include <sstream>

#include "core.h"

using namespace timerewind;

static int g_fail = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++g_fail;                                                  \
        }                                                              \
    } while (0)

static bool near(double a, double b, double eps = 0.01) { return std::fabs(a - b) < eps; }

// A state where x follows the clock, so interpolation results are predictable.
static Snapshot make(double t, float y = 0.0f) {
    Snapshot s;
    s.player_position = {static_cast<float>(10.0 * t), y, 0.0f};
    return s;
}

static const double kDt = 1.0 / 60.0;

// Records `seconds` of game time at 60 fps; returns the clock reached.
static double record(Controller& c, double seconds, float y = 0.0f) {
    Snapshot out;
    double t = c.now();
    const int frames = static_cast<int>(std::lround(seconds * 60.0));
    for (int i = 0; i < frames; ++i) {
        t += kDt;
        c.step(kDt, false, make(t, y), out);
    }
    return t;
}

static void test_rewind_in_real_time() {
    Config cfg;
    Controller c(cfg);
    Snapshot out;
    record(c, 3.0);
    const double newest = c.timeline().newest();
    CHECK(newest > 2.9 && newest <= 3.01);

    for (int i = 0; i < 60; ++i) CHECK(c.step(kDt, true, make(0), out));
    CHECK(c.mode() == Mode::Rewinding);
    CHECK(near(c.now(), newest - 1.0, 1e-6));
    CHECK(near(out.player_position.x, 10.0 * c.now()));
}

static void test_buffer_limit_and_clamp() {
    Config cfg;
    Controller c(cfg);
    Snapshot out;
    record(c, 20.0);
    CHECK(c.timeline().newest() - c.timeline().oldest() <= cfg.buffer_seconds + 0.2);

    for (int i = 0; i < 6000; ++i) c.step(kDt, true, make(0), out);
    CHECK(c.now() == c.timeline().oldest());
    CHECK(near(out.player_position.x, 10.0 * c.timeline().oldest()));
}

static void test_release_replaces_the_future() {
    Config cfg;
    Controller c(cfg);
    Snapshot out;
    record(c, 3.0, 0.0f);
    for (int i = 0; i < 60; ++i) c.step(kDt, true, make(0), out);
    const double tr = c.now();

    // Release the key and play on for one second with a different history (y = 999).
    for (int i = 0; i < 60; ++i) c.step(kDt, false, make(0, 999.0f), out);
    CHECK(c.mode() == Mode::Recording);

    Snapshot s;
    CHECK(c.timeline().sample(tr - 0.5, s));
    CHECK(s.player_position.y == 0.0f);  // the past before the release point is kept
    CHECK(c.timeline().sample(tr + 0.5, s));
    CHECK(near(s.player_position.y, 999.0f));  // the old future was replaced
    CHECK(near(c.timeline().newest(), c.now(), 0.04));
}

static void test_snapshot_lerp_policies() {
    Snapshot a, b;
    a.player_position = {0, 0, 0};
    b.player_position = {10, 20, 30};
    a.player_heading = 350.0f;
    b.player_heading = 10.0f;
    a.player_health = 100;
    b.player_health = 50;
    const Snapshot m = snapshot_lerp(a, b, 0.5f);
    CHECK(near(m.player_position.y, 10.0));
    CHECK(near(m.player_heading, 0.0) || near(m.player_heading, 360.0));  // shorter arc over 0
    CHECK(m.player_health == 100);                                        // integers step
}

static void test_empty_and_disabled() {
    Config cfg;
    Controller c(cfg);
    Snapshot out;
    CHECK(!c.step(kDt, true, make(0), out));  // nothing recorded yet: key does nothing
    CHECK(c.mode() == Mode::Recording);

    record(c, 1.0);
    CHECK(!c.timeline().empty());
    c.set_enabled(false);
    CHECK(c.mode() == Mode::Disabled);
    CHECK(c.timeline().empty());
    CHECK(!c.step(kDt, true, make(0), out));
    CHECK(c.timeline().empty());
    c.set_enabled(true);
    CHECK(c.mode() == Mode::Recording);
}

static void test_ini() {
    std::istringstream in(
        "[RewindTime]\n"
        "rewind_key_vk = 0x74\n"
        "buffer_seconds=1000\n"
        "; a comment\n"
        "unknown = 1\n"
        "sample_rate_hz = abc\n"
        "rewind_speed = 2\n");
    Config cfg;
    load_ini(in, cfg);
    CHECK(cfg.rewind_key_vk == 116);       // hex accepted
    CHECK(near(cfg.buffer_seconds, 60.0));  // clamped to the sheet's max
    CHECK(cfg.sample_rate_hz == 30);        // unparsable value keeps the default
    CHECK(near(cfg.rewind_speed, 2.0));
}

int main() {
    test_rewind_in_real_time();
    test_buffer_limit_and_clamp();
    test_release_replaces_the_future();
    test_snapshot_lerp_policies();
    test_empty_and_disabled();
    test_ini();
    if (g_fail == 0) {
        std::printf("All core tests passed.\n");
        return 0;
    }
    std::printf("%d check(s) failed.\n", g_fail);
    return 1;
}
