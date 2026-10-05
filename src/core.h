// Platform independent rewind core: a time-ordered buffer of snapshots and the
// controller that records while the key is up and rewinds while it is held.
// It never touches the game; the plugin feeds it snapshots and applies the result.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <istream>
#include <string>

#include "generated/config.h"
#include "generated/snapshot.h"
#include "types.h"

namespace timerewind {

// Snapshots ordered by time, oldest first, capped at `capacity` entries.
class Timeline {
public:
    explicit Timeline(std::size_t capacity) : cap_(capacity < 2 ? 2 : capacity) {}

    // Time must move forward; samples that do not are ignored.
    void push(double t, const Snapshot& s) {
        if (!buf_.empty() && t <= buf_.back().t) return;
        buf_.push_back({t, s});
        while (buf_.size() > cap_) buf_.pop_front();
    }

    bool empty() const { return buf_.empty(); }
    std::size_t size() const { return buf_.size(); }
    double oldest() const { return buf_.front().t; }
    double newest() const { return buf_.back().t; }

    // State at time `t`, interpolated between neighbours and clamped to the ends.
    bool sample(double t, Snapshot& out) const {
        if (buf_.empty()) return false;
        if (t <= buf_.front().t) {
            out = buf_.front().s;
            return true;
        }
        if (t >= buf_.back().t) {
            out = buf_.back().s;
            return true;
        }
        const auto hi = std::lower_bound(buf_.begin(), buf_.end(), t,
                                         [](const Entry& e, double v) { return e.t < v; });
        const auto lo = hi - 1;
        const float a = static_cast<float>((t - lo->t) / (hi->t - lo->t));
        out = snapshot_lerp(lo->s, hi->s, a);
        return true;
    }

    void truncate_after(double t) {
        while (!buf_.empty() && buf_.back().t > t) buf_.pop_back();
    }

    void clear() { buf_.clear(); }

private:
    struct Entry {
        double t;
        Snapshot s;
    };
    std::size_t cap_;
    std::deque<Entry> buf_;
};

enum class Mode { Recording, Rewinding, Disabled };

class Controller {
public:
    explicit Controller(const Config& cfg)
        : cfg_(cfg),
          line_(static_cast<std::size_t>(std::ceil(cfg.buffer_seconds * cfg.sample_rate_hz)) + 2) {}

    Mode mode() const { return mode_; }
    double now() const { return now_; }
    const Timeline& timeline() const { return line_; }

    // Disabling forgets everything; enabling starts a fresh recording.
    void set_enabled(bool on) {
        if (!on) {
            mode_ = Mode::Disabled;
            reset();
        } else if (mode_ == Mode::Disabled) {
            reset();
            mode_ = Mode::Recording;
        }
    }

    // One game frame, `dt` real seconds after the previous one.
    // While recording: stores `current` when a sample is due and returns false.
    // While rewinding: puts the state to apply into `out` and returns true.
    bool step(double dt, bool key_held, const Snapshot& current, Snapshot& out) {
        if (mode_ == Mode::Disabled || dt <= 0.0) return false;

        if (mode_ == Mode::Recording && key_held && !line_.empty()) {
            mode_ = Mode::Rewinding;
            now_ = line_.newest();
        } else if (mode_ == Mode::Rewinding && !key_held) {
            finish_rewind();
            mode_ = Mode::Recording;
        }

        if (mode_ == Mode::Rewinding) {
            now_ = std::max(line_.oldest(), now_ - dt * cfg_.rewind_speed);
            return line_.sample(now_, out);
        }

        now_ += dt;
        // The small epsilon keeps sampling steady despite floating point drift.
        if (line_.empty() || now_ - last_push_ + 1e-6 >= 1.0 / cfg_.sample_rate_hz) {
            line_.push(now_, current);
            last_push_ = now_;
        }
        return false;
    }

private:
    // Releasing the key makes this moment the new "present": the old future is dropped.
    void finish_rewind() {
        Snapshot at;
        if (!line_.sample(now_, at)) return;
        line_.truncate_after(now_);
        if (line_.empty() || now_ > line_.newest()) line_.push(now_, at);
        last_push_ = now_;
    }

    void reset() {
        line_.clear();
        now_ = 0.0;
        last_push_ = 0.0;
    }

    Config cfg_;
    Timeline line_;
    Mode mode_ = Mode::Recording;
    double now_ = 0.0;
    double last_push_ = 0.0;
};

inline std::string trim(const std::string& s) {
    const std::size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return std::string();
    const std::size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// key = value lines; [sections] and lines starting with ; or # are ignored,
// unknown keys and unparsable values are skipped (comments go on their own line).
inline void load_ini(std::istream& in, Config& cfg) {
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') continue;
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        config_set(cfg, trim(line.substr(0, eq)), trim(line.substr(eq + 1)));
    }
}

}  // namespace timerewind
