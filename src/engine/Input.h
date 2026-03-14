#pragma once
#include <array>
#include <cstdint>

// Tiny action-based input layer. The platform snapshots raw keys each frame,
// then gameplay asks for semantic actions like Confirm or Stun.
enum class Key : uint8_t {
    W, A, S, D,
    Escape,
    Tab,
    Return,
    R,
    Space,
    F11,
    Count
};

enum class Action : uint8_t {
    Up,
    Down,
    Left,
    Right,
    Confirm,
    Cancel,
    Restart,
    ToggleDebug,
    Stun,
    Count
};

class Input {
public:
    void BeginFrame();
    void SetKey(Key k, bool isDown);

    bool Down(Key k) const;
    bool Pressed(Key k) const;
    bool Released(Key k) const;

    bool Down(Action a) const;
    bool Pressed(Action a) const;
    bool Released(Action a) const;

private:
    struct ActionBind {
        Key primary;
        Key secondary;
    };

    static ActionBind Bind(Action a);

    std::array<bool, static_cast<size_t>(Key::Count)> m_curr{};
    std::array<bool, static_cast<size_t>(Key::Count)> m_prev{};
};
