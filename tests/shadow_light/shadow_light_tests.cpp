#include "ShadowSun.h"
#include <array>
#include <iostream>
#include <stdexcept>

using Direction = std::array<float, 3>;
static void Check(bool condition, const char *description) {
  if (!condition)
    throw std::runtime_error(description);
}
static Direction Sample(const ShadowSun &sun, float fraction) {
  Direction direction{};
  Check(sun.Sample(fraction, 0.6f, direction.data()),
        "automatic sun available");
  const float length =
      std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] +
                direction[2] * direction[2]);
  Check(std::abs(length - 1.0f) < 1e-6f && direction[1] <= -0.59999f,
        "unit direction and elevation floor");
  return direction;
}
static float Tip(const Direction &direction) {
  return -1000.0f * direction[0] / direction[1];
}
int main() {
  try {
    ShadowSun sun;
    Direction unused{};
    Check(!sun.Sample(1, 0.6f, unused.data()),
          "no history before a valid scene");
    sun.Begin(0x9000, 1, true, false);
    Check(Sample(sun, 0) == Sample(sun, 1), "first frame has no stale history");
    float previous = Tip(Sample(sun, 1));
    int movingFrames = 0, quantizedChanges = 0;
    std::array<int, 3> lastQuantized{};
    // A stationary camera and a tall stationary wall. Only time of day
    // advances: 20 game ticks/s, rendered at 60 fps. The old s8 light remains
    // unchanged over runs of rendered frames.
    for (int tick = 1; tick <= 40; ++tick) {
      const uint16_t day = 0x9000 + tick * 10;
      sun.Begin(day, 1, true, false);
      const double angle =
          (double(day) - 32768.0) * 6.28318530717958647692 / 65536.0;
      std::array<int, 3> quantized = {int(-std::sin(angle) * 120),
                                      int(std::cos(angle) * 120),
                                      int(std::cos(angle) * 20)};
      if (quantized != lastQuantized)
        ++quantizedChanges;
      lastQuantized = quantized;
      for (float fraction : {1.0f / 3.0f, 2.0f / 3.0f, 1.0f}) {
        const float tip = Tip(Sample(sun, fraction));
        Check(tip > previous && tip - previous < 1.0f,
              "continuous small shadow-tip steps at every render frame");
        previous = tip;
        ++movingFrames;
      }
    }
    Check(quantizedChanges < 40,
          "control reproduces s8 plateaus even across game ticks");
    const auto last = Sample(sun, 1);
    sun.Begin(sun.current, 1, true, false);
    Check(Sample(sun, 0.2f) == last && Sample(sun, 0.8f) == last,
          "paused time stays still");
    sun.Begin(65530, 2, true, true);
    sun.Begin(4, 2, true, true);
    Check(std::abs(Tip(Sample(sun, 1)) - Tip(Sample(sun, 0))) < 5,
          "midnight wraps along the short arc");
    sun.Begin(65530, 2, true, true);
    Check(std::abs(Tip(Sample(sun, 1)) - Tip(Sample(sun, 0))) < 5,
          "reverse time wraps along the short arc");
    sun.Begin(0x8000, 2, true, true);
    Check(Sample(sun, 0) == Sample(sun, 1), "large time jump resets history");
    sun.Begin(0x800A, 3, true, true);
    Check(Sample(sun, 0) == Sample(sun, 1), "scene change resets history");
    sun.Begin(0x8014, 3, true, false);
    Check(Sample(sun, 0) == Sample(sun, 1),
          "sun/moon selection never interpolates across antipodes");
    sun.Begin(0x801E, 3, false, false);
    Check(!sun.Sample(1, 0.6f, unused.data()),
          "authored and disabled lighting use existing path");
    sun.Begin(0x8028, 3, true, false);
    Check(Sample(sun, 0) == Sample(sun, 1), "reenable resets history");
    std::cout << movingFrames
              << " continuously moving render frames; s8 light changed on "
              << quantizedChanges
              << "/40 game ticks. Pause/wrap/reset checks passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
