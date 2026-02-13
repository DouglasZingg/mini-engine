#include "engine/Input.h"

void Input::SetKey(Key k, bool isDown) {
    m_state.down[(int)k] = isDown;
}

bool Input::Down(Key k) const {
    return m_state.down[(int)k];
}
