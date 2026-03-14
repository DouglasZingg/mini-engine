#include "engine/Input.h"

namespace {
bool IsValidKey(Key k) {
    return k != Key::Count;
}
}

void Input::BeginFrame() {
    m_prev = m_curr;
}

void Input::SetKey(Key k, bool isDown) {
    m_curr[static_cast<size_t>(k)] = isDown;
}

bool Input::Down(Key k) const {
    return m_curr[static_cast<size_t>(k)];
}

bool Input::Pressed(Key k) const {
    return m_curr[static_cast<size_t>(k)] && !m_prev[static_cast<size_t>(k)];
}

bool Input::Released(Key k) const {
    return !m_curr[static_cast<size_t>(k)] && m_prev[static_cast<size_t>(k)];
}

Input::ActionBind Input::Bind(Action a) {
    switch (a) {
    case Action::Up:          return { Key::W, Key::Count };
    case Action::Down:        return { Key::S, Key::Count };
    case Action::Left:        return { Key::A, Key::Count };
    case Action::Right:       return { Key::D, Key::Count };
    case Action::Confirm:     return { Key::Return, Key::Count };
    case Action::Cancel:      return { Key::Escape, Key::Count };
    case Action::Restart:     return { Key::R, Key::Count };
    case Action::ToggleDebug: return { Key::Tab, Key::Count };
    case Action::Stun:        return { Key::Space, Key::Count };
    default:                  return { Key::Count, Key::Count };
    }
}

bool Input::Down(Action a) const {
    const ActionBind bind = Bind(a);
    return (IsValidKey(bind.primary) && Down(bind.primary)) ||
           (IsValidKey(bind.secondary) && Down(bind.secondary));
}

bool Input::Pressed(Action a) const {
    const ActionBind bind = Bind(a);
    return (IsValidKey(bind.primary) && Pressed(bind.primary)) ||
           (IsValidKey(bind.secondary) && Pressed(bind.secondary));
}

bool Input::Released(Action a) const {
    const ActionBind bind = Bind(a);
    return (IsValidKey(bind.primary) && Released(bind.primary)) ||
           (IsValidKey(bind.secondary) && Released(bind.secondary));
}
