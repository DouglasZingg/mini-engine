#include "engine/Input.h"

void Input::BeginFrame() {
    m_prev = m_curr;
}

void Input::SetKey(Key k, bool isDown) {
    m_curr[(size_t)k] = isDown;
}

bool Input::Down(Key k) const {
    return m_curr[(size_t)k];
}

bool Input::Pressed(Key k) const {
    return m_curr[(size_t)k] && !m_prev[(size_t)k];
}

bool Input::Released(Key k) const {
    return !m_curr[(size_t)k] && m_prev[(size_t)k];
}

Input::ActionBind Input::Bind(Action a) {
    switch (a) {
    case Action::Up:         return { Key::W, Key::Count };
    case Action::Down:       return { Key::S, Key::Count };
    case Action::Left:       return { Key::A, Key::Count };
    case Action::Right:      return { Key::D, Key::Count };
    case Action::Confirm:    return { Key::Return, Key::Count };
    case Action::Cancel:     return { Key::Escape, Key::Count };
    case Action::Restart:    return { Key::R, Key::Count };
    case Action::ToggleDebug:return { Key::Tab, Key::Count };
    case Action::Stun:       return { Key::Space, Key::Count };
    default:                 return { Key::Count, Key::Count };
    }
}

static bool key_valid(Key k) { return k != Key::Count; }

bool Input::Down(Action a) const {
    const auto b = Bind(a);
    bool d = false;
    if (key_valid(b.primary))   d |= Down(b.primary);
    if (key_valid(b.secondary)) d |= Down(b.secondary);
    return d;
}

bool Input::Pressed(Action a) const {
    const auto b = Bind(a);
    bool p = false;
    if (key_valid(b.primary))   p |= Pressed(b.primary);
    if (key_valid(b.secondary)) p |= Pressed(b.secondary);
    return p;
}

bool Input::Released(Action a) const {
    const auto b = Bind(a);
    bool r = false;
    if (key_valid(b.primary))   r |= Released(b.primary);
    if (key_valid(b.secondary)) r |= Released(b.secondary);
    return r;
}
