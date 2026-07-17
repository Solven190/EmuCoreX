package com.sbro.emucorex.navigation

import androidx.compose.material3.Text
import androidx.compose.ui.test.junit4.v2.createComposeRule
import androidx.compose.ui.test.junit4.StateRestorationTester
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.sbro.emucorex.ui.theme.EmuCoreXTheme
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class AdaptiveShellDrawerInstrumentedTest {
    @get:Rule
    val composeRule = createComposeRule()

    @Test
    fun feedbackDestinationDoesNotComposeModalDrawer() {
        composeRule.setContent {
            EmuCoreXTheme {
                TestShell(selected = PrimaryDestination.Feedback, onBackClick = {})
            }
        }

        val drawerNodes = composeRule
            .onAllNodesWithTag("adaptive_shell_modal_drawer", useUnmergedTree = true)
            .fetchSemanticsNodes()
        assertEquals(0, drawerNodes.size)
    }

    @Test
    fun homeDestinationStillComposesModalDrawer() {
        composeRule.setContent {
            EmuCoreXTheme {
                TestShell(selected = PrimaryDestination.Home, onBackClick = null)
            }
        }

        val drawerNodes = composeRule
            .onAllNodesWithTag("adaptive_shell_modal_drawer", useUnmergedTree = true)
            .fetchSemanticsNodes()
        assertEquals(1, drawerNodes.size)
    }

    @Test
    fun feedbackDestinationStaysDrawerFreeAfterStateRestoration() {
        val restorationTester = StateRestorationTester(composeRule)
        restorationTester.setContent {
            EmuCoreXTheme {
                TestShell(selected = PrimaryDestination.Feedback, onBackClick = {})
            }
        }

        assertEquals(0, modalDrawerNodeCount())
        restorationTester.emulateSavedInstanceStateRestore()
        assertEquals(0, modalDrawerNodeCount())
    }

    private fun modalDrawerNodeCount(): Int = composeRule
        .onAllNodesWithTag("adaptive_shell_modal_drawer", useUnmergedTree = true)
        .fetchSemanticsNodes()
        .size
}

@androidx.compose.runtime.Composable
private fun TestShell(selected: PrimaryDestination, onBackClick: (() -> Unit)?) {
    AdaptiveShell(
        selected = selected,
        onNavigateHome = {},
        onNavigateSearch = {},
        onNavigateFormats = {},
        onNavigateSettings = {},
        onNavigateAchievements = {},
        onBackClick = onBackClick
    ) {
        Text("Test content")
    }
}
