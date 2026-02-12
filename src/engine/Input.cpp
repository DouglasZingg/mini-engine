#include "engine/Input.h"
#include <cassert>

void Input::SetKey(Key k, bool isDown) {
    const int idx = (int)k;
    assert(idx >= 0 && idx < (int)Key::Count);
    m_state.down[idx] = isDown;
}

bool Input::Down(Key k) const {
    const int idx = (int)k;
    assert(idx >= 0 && idx < (int)Key::Count);
    return m_state.down[idx];
}
