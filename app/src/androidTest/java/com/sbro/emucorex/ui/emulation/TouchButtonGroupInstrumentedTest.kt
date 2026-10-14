package com.sbro.emucorex.ui.emulation

import androidx.compose.foundation.shape.CircleShape
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.test.junit4.v2.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.unit.dp
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.sbro.emucorex.R
import com.sbro.emucorex.data.TouchControlPressEffect
import com.sbro.emucorex.data.TouchControlVisualStyle
import com.sbro.emucorex.ui.theme.EmuCoreXTheme
import java.util.Collections
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class TouchButtonGroupInstrumentedTest {
    @get:Rule
    val composeRule = createComposeRule()

    private val events = Collections.synchronizedList(mutableListOf<String>())
    private val crossOwner = mutableStateOf("cross")

    private fun showButtons(sharedAction: Boolean = false, stickyCross: Boolean = false, longPressCircle: Boolean = false) {
        composeRule.setContent {
            EmuCoreXTheme {
                val owner = crossOwner.value
                val cross = TouchButtonPressAction(owner) { events += "$owner:$it" }
                val circle = TouchButtonPressAction("circle") { events += "circle:$it" }
                TouchButtonGroup(
                    specs = listOf(
                        TouchButtonSpec("cross", drawableRes = R.drawable.ic_launcher_foreground,
                            width = 100.dp, height = 100.dp, x = 40.dp, y = 40.dp,
                            shape = CircleShape, pressActions = listOf(cross), tapToHold = stickyCross),
                        TouchButtonSpec("circle", drawableRes = R.drawable.ic_launcher_foreground,
                            width = 100.dp, height = 100.dp, x = 100.dp, y = 40.dp,
                            shape = CircleShape, pressActions = if (sharedAction) listOf(circle, cross) else listOf(circle),
                            longPressDelayMs = if (longPressCircle) 100L else 0L,
                            onLongPressChange = if (longPressCircle) { pressed -> events += "hold:$pressed" } else null)
                    ),
                    inputWidth = 300.dp,
                    inputHeight = 200.dp,
                    touchscreenRightStick = false,
                    touchscreenRightStickSensitivity = 1f,
                    invertRightStick = false,
                    invertRightStickHorizontal = false,
                    onPadInput = { _, _, _ -> },
                    visualStyle = TouchControlVisualStyle.CLASSIC,
                    pressEffect = TouchControlPressEffect.GROW,
                    onTouchHaptic = {},
                    modifier = Modifier.testTag("touch_buttons")
                )
            }
        }
    }

    private fun point(x: Float): Offset {
        val size = composeRule.onNodeWithTag("touch_buttons").fetchSemanticsNode().size
        return Offset(size.width * x / 300f, size.height * 90f / 200f)
    }

    @Test
    fun overlapPressesBothAndDragReleasesOnlyExitedButton() {
        showButtons()
        val overlap = point(120f)
        val circleOnly = point(170f)
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { down(overlap) }
        composeRule.runOnIdle { assertEquals(listOf("cross:true", "circle:true"), events) }
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { moveTo(circleOnly) }
        composeRule.runOnIdle { assertEquals(listOf("cross:true", "circle:true", "cross:false"), events) }
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { up() }
        composeRule.runOnIdle { assertEquals(listOf("cross:true", "circle:true", "cross:false", "circle:false"), events) }
    }

    @Test
    fun twoFingersKeepSharedActionDownUntilLastOwnerReleases() {
        showButtons(sharedAction = true)
        val crossOnly = point(70f)
        val circleOnly = point(170f)
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { down(0, crossOnly); down(1, circleOnly) }
        composeRule.runOnIdle { assertEquals(listOf("cross:true", "circle:true"), events) }
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { up(0) }
        composeRule.runOnIdle { assertEquals(listOf("cross:true", "circle:true"), events) }
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { up(1) }
        composeRule.runOnIdle {
            assertEquals(listOf("cross:true", "circle:true", "circle:false", "cross:false"), events)
        }
    }

    @Test
    fun pointerCancellationReleasesAllOverlappingActions() {
        showButtons()
        val overlap = point(120f)
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { down(overlap); cancel() }
        composeRule.runOnIdle { assertEquals(listOf("cross:true", "circle:true", "cross:false", "circle:false"), events) }
    }

    @Test
    fun stickyButtonTogglesWhileOtherOverlappingButtonRemainsMomentary() {
        showButtons(stickyCross = true)
        val overlap = point(120f)
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { down(overlap); up() }
        composeRule.runOnIdle { assertEquals(listOf("circle:true", "circle:false", "cross:true"), events) }
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { down(overlap); up() }
        composeRule.runOnIdle {
            assertEquals(listOf("circle:true", "circle:false", "cross:true", "circle:true", "circle:false", "cross:false"), events)
        }
    }

    @Test
    fun cancellationDoesNotLatchStickyButton() {
        showButtons(stickyCross = true)
        val overlap = point(120f)
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { down(overlap); cancel() }
        composeRule.runOnIdle { assertEquals(listOf("circle:true", "circle:false"), events) }
    }

    @Test
    fun stickyAndMomentaryTargetsSharingActionKeepItContinuousOnRelease() {
        showButtons(sharedAction = true, stickyCross = true)
        val overlap = point(120f)
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { down(overlap); up() }
        composeRule.runOnIdle { assertEquals(listOf("circle:true", "cross:true", "circle:false"), events) }
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { down(overlap); up() }
        composeRule.runOnIdle {
            assertEquals(listOf("circle:true", "cross:true", "circle:false", "circle:true", "circle:false", "cross:false"), events)
        }
    }

    @Test
    fun cancellationDoesNotEmitShortTap() {
        showButtons(longPressCircle = true)
        val overlap = point(120f)
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { down(overlap); cancel() }
        composeRule.runOnIdle { assertEquals(listOf("cross:true", "cross:false"), events) }
    }

    @Test
    fun shortTapAndMomentaryTargetsSharingActionKeepItContinuousOnRelease() {
        showButtons(sharedAction = true, longPressCircle = true)
        val overlap = point(120f)
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { down(overlap); up() }
        composeRule.waitUntil(2_000) { events.contains("circle:false") }
        composeRule.runOnIdle {
            assertEquals(listOf("cross:true", "circle:true", "circle:false", "cross:false"), events)
        }
    }

    @Test
    fun longPressDoesNotSuppressOtherOverlappingButtonOrEmitShortTap() {
        showButtons(longPressCircle = true)
        val overlap = point(120f)
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { down(overlap) }
        composeRule.waitUntil(2_000) { events.contains("hold:true") }
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { up() }
        composeRule.runOnIdle { assertEquals(listOf("cross:true", "hold:true", "hold:false", "cross:false"), events) }
    }

    @Test
    fun rebindingHeldButtonReleasesOriginalActionBeforeNewMapping() {
        showButtons()
        val crossOnly = point(70f)
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { down(crossOnly) }
        composeRule.runOnIdle { crossOwner.value = "square" }
        composeRule.waitForIdle()
        composeRule.runOnIdle { assertEquals(listOf("cross:true", "cross:false"), events) }
        composeRule.onNodeWithTag("touch_buttons").performTouchInput { up() }
        composeRule.runOnIdle { assertEquals(listOf("cross:true", "cross:false"), events) }
    }
}
