#pragma once
#include <cstdint>
#include <array>

/*
    Input is intentionally small and boring:
    - Platform snapshots raw key states each frame.
    - We keep prev/current so "Pressed/Released" is reliable.
    - Actions are just a tiny mapping layer so the game code doesn't care about SDL scancodes.
*/

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
    // Call once per frame BEFORE updating key states.
    void BeginFrame();

    // Platform updates keys with the current snapshot.
    void SetKey(Key k, bool isDown);

    // Call after the first fixed-step update in a rendered frame so Pressed/Released do not
    // repeat when the game needs to catch up with multiple simulation steps.
    void ConsumeTransient();

    // Raw key queries
    bool Down(Key k) const;
    bool Pressed(Key k) const;
    bool Released(Key k) const;

    // Action queries (mapped keys)
    bool Down(Action a) const;
    bool Pressed(Action a) const;
    bool Released(Action a) const;

private:
    std::array<bool, (size_t)Key::Count> m_curr{};
    std::array<bool, (size_t)Key::Count> m_prev{};

    // action -> up to 2 keys (we only use 1 right now, but this makes rebinding easy later)
    struct ActionBind { Key primary; Key secondary; };
    static ActionBind Bind(Action a);
};
