package com.sbro.emucorex.data

private const val GAMEPAD_BINDINGS_KEY = "gamepadBindingsByPad"

/** Updates only controller bindings, retaining every other per-game override. */
fun PerGameSettings.withGamepadBindingsByPad(
    bindingsByPad: Map<Int, Map<String, Int>>
): PerGameSettings {
    val updatedKeys = providedKeys?.let { keys ->
        if (bindingsByPad.isEmpty()) keys - GAMEPAD_BINDINGS_KEY else keys + GAMEPAD_BINDINGS_KEY
    }
    return copy(gamepadBindingsByPad = bindingsByPad, providedKeys = updatedKeys)
}
