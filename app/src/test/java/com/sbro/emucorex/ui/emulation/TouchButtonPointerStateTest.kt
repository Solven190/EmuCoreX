package com.sbro.emucorex.ui.emulation

import org.junit.Assert.assertEquals
import org.junit.Test

class TouchButtonPointerStateTest {
    @Test
    fun `one pointer activates every intersecting target and action`() {
        val state = TouchButtonPointerState(
            mapOf(
                "cross" to setOf("pad-cross"),
                "circle" to setOf("pad-circle")
            )
        )

        val transition = state.update(1L, listOf("cross", "circle"))

        assertEquals(setOf("cross", "circle"), transition.enteredTargets)
        assertEquals(setOf("pad-cross", "pad-circle"), transition.pressedActionOwners)
        assertEquals(setOf("cross", "circle"), transition.activeTargets)
    }

    @Test
    fun `same logical action stays pressed until its last target pointer releases`() {
        val state = TouchButtonPointerState(
            mapOf(
                "cross" to setOf("pad-cross"),
                "custom-cross" to setOf("pad-cross")
            )
        )

        val firstDown = state.update(1L, listOf("cross"))
        val secondDown = state.update(2L, listOf("custom-cross"))
        val firstUp = state.update(1L, emptyList())
        val lastUp = state.update(2L, emptyList())

        assertEquals(setOf("pad-cross"), firstDown.pressedActionOwners)
        assertEquals(setOf("cross"), firstDown.activatedTargets)
        assertEquals(emptySet<String>(), secondDown.pressedActionOwners)
        assertEquals(setOf("custom-cross"), secondDown.activatedTargets)
        assertEquals(emptySet<String>(), firstUp.releasedActionOwners)
        assertEquals(setOf("cross"), firstUp.deactivatedTargets)
        assertEquals(setOf("pad-cross"), lastUp.releasedActionOwners)
        assertEquals(setOf("custom-cross"), lastUp.deactivatedTargets)
    }

    @Test
    fun `same target held by two pointers is released only after both lift`() {
        val state = TouchButtonPointerState(mapOf("cross" to setOf("pad-cross")))

        val firstDown = state.update(1L, listOf("cross"))
        val secondDown = state.update(2L, listOf("cross"))
        val firstUp = state.update(1L, emptyList())
        val lastUp = state.update(2L, emptyList())

        assertEquals(setOf("pad-cross"), firstDown.pressedActionOwners)
        assertEquals(emptySet<String>(), secondDown.pressedActionOwners)
        assertEquals(emptySet<String>(), firstUp.releasedActionOwners)
        assertEquals(setOf("pad-cross"), lastUp.releasedActionOwners)
    }

    @Test
    fun `sticky owner shares reference count with an actively held overlapping action`() {
        val state = TouchButtonPointerState(
            actionOwnersByTarget = mapOf(
                "cross" to setOf("pad-cross"),
                "sticky-cross" to setOf("pad-cross")
            ),
            pointerActionTargets = setOf("cross")
        )

        val fingerDown = state.update(1L, listOf("cross"))
        val stickyOn = state.setLatched("sticky-cross", latched = true)
        val fingerUp = state.update(1L, emptyList())
        val stickyOff = state.setLatched("sticky-cross", latched = false)

        assertEquals(setOf("pad-cross"), fingerDown.pressedActionOwners)
        assertEquals(emptySet<String>(), stickyOn.pressedActionOwners)
        assertEquals(emptySet<String>(), fingerUp.releasedActionOwners)
        assertEquals(setOf("pad-cross"), stickyOff.releasedActionOwners)
    }

    @Test
    fun `short tap transient keeps shared action held until the last source ends`() {
        val state = TouchButtonPointerState(mapOf("cross" to setOf("pad-cross")))
        state.update(1L, listOf("cross"))

        val pulseStart = state.setTransientActionOwners("tap-1", setOf("pad-cross"))
        val pointerUp = state.update(1L, emptyList())
        val secondPulse = state.setTransientActionOwners("tap-2", setOf("pad-cross"))
        val firstPulseEnd = state.setTransientActionOwners("tap-1", emptySet())
        val finalPulseEnd = state.setTransientActionOwners("tap-2", emptySet())

        assertEquals(emptySet<String>(), pulseStart.pressedActionOwners)
        assertEquals(emptySet<String>(), pointerUp.releasedActionOwners)
        assertEquals(emptySet<String>(), secondPulse.pressedActionOwners)
        assertEquals(emptySet<String>(), firstPulseEnd.releasedActionOwners)
        assertEquals(setOf("pad-cross"), finalPulseEnd.releasedActionOwners)
    }

    @Test
    fun `pointer completion transfers action directly to sticky latch`() {
        val state = TouchButtonPointerState(
            mapOf("cross" to setOf("pad-cross"), "sticky-cross" to setOf("pad-cross")),
            pointerActionTargets = setOf("cross")
        )
        state.update(1L, listOf("cross"))

        val completion = state.completePointer(1L, latchedUpdates = mapOf("sticky-cross" to true))

        assertEquals(emptySet<String>(), completion.releasedActionOwners)
        assertEquals(emptySet<String>(), completion.pressedActionOwners)
    }

    @Test
    fun `pointer completion transfers action directly to short tap pulse`() {
        val state = TouchButtonPointerState(
            mapOf("cross" to setOf("pad-cross"), "start" to setOf("pad-cross")),
            pointerActionTargets = setOf("cross")
        )
        state.update(1L, listOf("cross"))

        val completion = state.completePointer(
            pointerId = 1L,
            transientActionOwners = mapOf("tap:1:start:1" to setOf("pad-cross"))
        )

        assertEquals(emptySet<String>(), completion.releasedActionOwners)
        assertEquals(emptySet<String>(), completion.pressedActionOwners)
    }

    @Test
    fun `overlapping short tap pulses do not release each other early`() {
        val state = TouchButtonPointerState(emptyMap())

        val first = state.setTransientActionOwners("tap-1", setOf("pad-start"))
        val second = state.setTransientActionOwners("tap-2", setOf("pad-start"))
        val firstEnd = state.setTransientActionOwners("tap-1", emptySet())
        val secondEnd = state.setTransientActionOwners("tap-2", emptySet())

        assertEquals(setOf("pad-start"), first.pressedActionOwners)
        assertEquals(emptySet<String>(), second.pressedActionOwners)
        assertEquals(emptySet<String>(), firstEnd.releasedActionOwners)
        assertEquals(setOf("pad-start"), secondEnd.releasedActionOwners)
    }

    @Test
    fun `drag transitions only the target and action differences`() {
        val state = TouchButtonPointerState(
            mapOf(
                "cross" to setOf("pad-cross"),
                "circle" to setOf("pad-circle")
            )
        )
        state.update(7L, listOf("cross"))

        val transition = state.update(7L, listOf("cross", "circle"))

        assertEquals(setOf("circle"), transition.enteredTargets)
        assertEquals(emptySet<String>(), transition.exitedTargets)
        assertEquals(setOf("pad-circle"), transition.pressedActionOwners)
        assertEquals(emptySet<String>(), transition.releasedActionOwners)
        assertEquals(setOf("cross", "circle"), transition.activeTargets)
    }

    @Test
    fun `drag between targets sharing a secondary action does not release that action`() {
        val state = TouchButtonPointerState(
            mapOf(
                "primary-cross" to setOf("pad-cross"),
                "custom-with-cross-secondary" to setOf("pad-cross", "pad-start")
            )
        )
        state.update(7L, listOf("primary-cross"))

        val transition = state.update(7L, listOf("custom-with-cross-secondary"))

        assertEquals(setOf("primary-cross"), transition.exitedTargets)
        assertEquals(setOf("custom-with-cross-secondary"), transition.enteredTargets)
        assertEquals(emptySet<String>(), transition.releasedActionOwners)
        assertEquals(setOf("pad-start"), transition.pressedActionOwners)
    }

    @Test
    fun `release and cancellation share exact final ownership transition`() {
        val state = TouchButtonPointerState(mapOf("square" to setOf("pad-square")))
        state.update(9L, listOf("square"))

        val release = state.update(9L, emptyList())

        assertEquals(setOf("square"), release.exitedTargets)
        assertEquals(setOf("pad-square"), release.releasedActionOwners)
        assertEquals(emptySet<String>(), release.activeTargets)
    }

    @Test
    fun `repeated cancellation is idempotent and releases no unrelated pointer`() {
        val state = TouchButtonPointerState(
            mapOf("cross" to setOf("pad-cross"), "circle" to setOf("pad-circle"))
        )
        state.update(1L, listOf("cross"))
        state.update(2L, listOf("circle"))

        val cancelFirst = state.update(1L, emptyList())
        val repeatedCancel = state.update(1L, emptyList())

        assertEquals(setOf("pad-cross"), cancelFirst.releasedActionOwners)
        assertEquals(emptySet<String>(), repeatedCancel.releasedActionOwners)
        assertEquals(setOf("circle"), repeatedCancel.activeTargets)
    }

    @Test
    fun `click-only utility on top remains exclusive over a gameplay hitbox`() {
        val targets = selectTouchButtonHitTargets(
            listOf(
                TouchButtonHitCandidate("cross", clickOnlyUtility = false),
                TouchButtonHitCandidate("left_input_toggle", clickOnlyUtility = true)
            )
        )

        assertEquals(listOf("left_input_toggle"), targets)
    }

    @Test
    fun `overlapping gameplay hitboxes remain in stable draw order`() {
        val targets = selectTouchButtonHitTargets(
            listOf(
                TouchButtonHitCandidate("cross", clickOnlyUtility = false),
                TouchButtonHitCandidate("circle", clickOnlyUtility = false)
            )
        )

        assertEquals(listOf("cross", "circle"), targets)
    }

    @Test
    fun `tap sticky and short tap targets require same button at down and up`() {
        assertEquals(
            setOf("cross", "circle"),
            matchingTouchButtonReleaseTargets(
                downTargets = setOf("cross", "circle"),
                upTargets = setOf("cross", "circle")
            )
        )
        assertEquals(
            emptySet<String>(),
            matchingTouchButtonReleaseTargets(
                downTargets = setOf("cross"),
                upTargets = setOf("circle")
            )
        )
    }
}
