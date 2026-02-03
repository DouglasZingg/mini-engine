#pragma once
#include <cstdint>

enum class Key : uint8_t {
    W, A, S, D,
    Escape,
    Count
};

struct InputState {
    bool down[(int)Key::Count] = {};
};

class Input {
public:
    void SetKey(Key k, bool isDown);
    bool Down(Key k) const;

private:
    InputState m_state{};
};
