#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// The environment stores the sun in s8 lighting commands. Shadows need its continuous direction:
// a one-unit change in those commands moves the shadow of a tall wall by several world units.
struct ShadowSun {
    bool valid = false;
    bool moon = false;
    int scene = -1;
    uint16_t current = 0;
    float previous = 0;

    void Begin(uint16_t dayTime, int sceneId, bool automatic, bool useMoon) {
        int delta = int(dayTime) - int(current);
        if (delta > 32767)
            delta -= 65536;
        if (delta < -32768)
            delta += 65536;
        const bool continuous = valid && automatic && scene == sceneId && moon == useMoon && std::abs(delta) <= 0x1000;
        previous = continuous ? float(dayTime) - float(delta) : float(dayTime);
        current = dayTime;
        scene = sceneId;
        moon = useMoon;
        valid = automatic;
    }

    bool Sample(float fraction, float minElevation, float out[3]) const {
        if (!valid)
            return false;
        const double day = previous + (float(current) - previous) * std::clamp(fraction, 0.0f, 1.0f);
        const double angle = (day - 32768.0) * (6.28318530717958647692 / 65536.0);
        const double sign = moon ? -1.0 : 1.0;
        // Same orbit as Environment_Update: (-sin(t)*120, cos(t)*120, cos(t)*20).
        double x = -std::sin(angle) * 120.0 * sign;
        double y = std::cos(angle) * 120.0 * sign;
        double z = std::cos(angle) * 20.0 * sign;
        const double length = std::sqrt(x * x + y * y + z * z);
        x /= length;
        y /= length;
        z /= length;
        const double elevation = std::max(y, double(std::clamp(minElevation, 0.05f, 0.99f)));
        const double horizontal = std::sqrt(x * x + z * z);
        const double scale = horizontal > 1e-9 ? std::sqrt(1.0 - elevation * elevation) / horizontal : 0.0;
        // Renderer convention: direction travelled by light, opposite to the environment's key.
        out[0] = float(-x * scale);
        out[1] = float(-elevation);
        out[2] = float(-z * scale);
        return true;
    }
};
