#pragma once

struct TimeStep {
    float dt = 0.0f;          // variable dt (per-frame)
    float fixedDt = 1.0f / 60.0f; // fixed dt for simulation
    float alpha = 0.0f;       // interpolation factor (0..1)
};
