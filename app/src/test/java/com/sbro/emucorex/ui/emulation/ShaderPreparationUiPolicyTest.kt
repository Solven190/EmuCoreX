package com.sbro.emucorex.ui.emulation

import com.sbro.emucorex.core.RendererDefaults
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ShaderPreparationUiPolicyTest {
    @Test
    fun onlyNativeValidatedReadinessSuppressesTheOffer() {
        val launch = mapOf(
            "renderer" to RendererDefaults.VULKAN,
            "bootToBios" to 0,
            "bootSmokeProbe" to 0,
            "autotestMode" to 0,
            "restoredAfterProcessDeath" to 0
        )

        fun offer(readiness: ShaderPreparationReadiness) = ShaderPreparationUiPolicy.shouldOffer(
            renderer = launch.getValue("renderer"),
            bootToBios = launch.getValue("bootToBios") != 0,
            bootSmokeProbe = launch.getValue("bootSmokeProbe") != 0,
            autotestMode = launch.getValue("autotestMode") != 0,
            restoredAfterProcessDeath = launch.getValue("restoredAfterProcessDeath") != 0,
            readiness = readiness
        )

        assertFalse(offer(ShaderPreparationReadiness.ValidatedPrepared))
        assertTrue(offer(ShaderPreparationReadiness.NeedsPreparation))
        assertTrue(offer(ShaderPreparationReadiness.Unavailable))
    }

    @Test
    fun preparationOfferIsLimitedToOrdinaryVulkanGameLaunches() {
        assertTrue(ShaderPreparationUiPolicy.shouldOffer(
            renderer = RendererDefaults.VULKAN,
            bootToBios = false,
            bootSmokeProbe = false,
            autotestMode = false,
            restoredAfterProcessDeath = false
        ))

        assertFalse(ShaderPreparationUiPolicy.shouldOffer(
            renderer = RendererDefaults.OPENGL,
            bootToBios = false,
            bootSmokeProbe = false,
            autotestMode = false,
            restoredAfterProcessDeath = false
        ))
        assertFalse(ShaderPreparationUiPolicy.shouldOffer(
            renderer = RendererDefaults.VULKAN,
            bootToBios = true,
            bootSmokeProbe = false,
            autotestMode = false,
            restoredAfterProcessDeath = false
        ))
        assertFalse(ShaderPreparationUiPolicy.shouldOffer(
            renderer = RendererDefaults.VULKAN,
            bootToBios = false,
            bootSmokeProbe = true,
            autotestMode = false,
            restoredAfterProcessDeath = false
        ))
        assertFalse(ShaderPreparationUiPolicy.shouldOffer(
            renderer = RendererDefaults.VULKAN,
            bootToBios = false,
            bootSmokeProbe = false,
            autotestMode = true,
            restoredAfterProcessDeath = false
        ))
        assertFalse(ShaderPreparationUiPolicy.shouldOffer(
            renderer = RendererDefaults.VULKAN,
            bootToBios = false,
            bootSmokeProbe = false,
            autotestMode = false,
            restoredAfterProcessDeath = true
        ))
    }

    @Test
    fun progressUsesNativeCountsAndDoesNotInventCompletionForZeroTotal() {
        val progress = ShaderPreparationUiPolicy.progress(
            ShaderPreparationUiSnapshot(
                phase = ShaderPreparationUiPhase.Running,
                total = 164,
                completed = 12,
                alreadyAvailable = 23,
                failed = 2,
                cancelled = 1,
                inFlight = 3,
                finished = false,
                checkpointSaved = false
            )
        )

        assertEquals(35, progress.ready)
        assertEquals(164, progress.total)
        assertEquals(35f / 164f, progress.fraction!!, 0.0001f)
        assertFalse(ShaderPreparationUiPolicy.progress(
            ShaderPreparationUiSnapshot(
                phase = ShaderPreparationUiPhase.Running,
                total = 0,
                completed = 0,
                alreadyAvailable = 0,
                failed = 0,
                cancelled = 0,
                inFlight = 0,
                finished = false,
                checkpointSaved = false
            )
        ).fraction != null)
    }

    @Test
    fun terminalActionsKeepCancelResumeAndContinueSemanticsDistinct() {
        assertEquals(
            setOf(ShaderPreparationUiAction.Prepare, ShaderPreparationUiAction.Skip),
            ShaderPreparationUiPolicy.actions(ShaderPreparationUiPhase.Offer)
        )
        assertEquals(
            setOf(ShaderPreparationUiAction.Cancel),
            ShaderPreparationUiPolicy.actions(ShaderPreparationUiPhase.Running)
        )
        assertEquals(
            setOf(ShaderPreparationUiAction.Resume, ShaderPreparationUiAction.ContinueToGame),
            ShaderPreparationUiPolicy.actions(ShaderPreparationUiPhase.Cancelled)
        )
        assertEquals(
            setOf(ShaderPreparationUiAction.Resume, ShaderPreparationUiAction.ContinueToGame),
            ShaderPreparationUiPolicy.actions(ShaderPreparationUiPhase.Failed)
        )
        assertEquals(
            setOf(ShaderPreparationUiAction.ContinueToGame),
            ShaderPreparationUiPolicy.actions(ShaderPreparationUiPhase.Unsupported)
        )
        assertEquals(
            setOf(ShaderPreparationUiAction.ContinueToGame),
            ShaderPreparationUiPolicy.actions(ShaderPreparationUiPhase.Completed)
        )
        assertEquals(
            setOf(ShaderPreparationUiAction.ContinueToGame),
            ShaderPreparationUiPolicy.actions(
                ShaderPreparationUiSnapshot(
                    phase = ShaderPreparationUiPhase.Failed,
                    total = 0,
                    completed = 0,
                    alreadyAvailable = 0,
                    failed = 0,
                    cancelled = 0,
                    inFlight = 0,
                    finished = false,
                    checkpointSaved = false
                )
            )
        )
    }

    @Test
    fun onlyCheckpointPublicationIsPresentedAsDurableReadiness() {
        assertTrue(ShaderPreparationUiPolicy.isDurablyReady(
            ShaderPreparationUiSnapshot(
                phase = ShaderPreparationUiPhase.Completed,
                total = 164,
                completed = 164,
                alreadyAvailable = 0,
                failed = 0,
                cancelled = 0,
                inFlight = 0,
                finished = true,
                checkpointSaved = true
            )
        ))
        assertFalse(ShaderPreparationUiPolicy.isDurablyReady(
            ShaderPreparationUiSnapshot(
                phase = ShaderPreparationUiPhase.Completed,
                total = 164,
                completed = 164,
                alreadyAvailable = 0,
                failed = 0,
                cancelled = 0,
                inFlight = 0,
                finished = true,
                checkpointSaved = false
            )
        ))
    }

    @Test
    fun unknownWorkerOwnerOffersOnlyCancelCleanupAndNeverContinue() {
        val actions = ShaderPreparationUiPolicy.actions(
            ShaderPreparationUiSnapshot(
                phase = ShaderPreparationUiPhase.Failed,
                total = 0,
                completed = 0,
                alreadyAvailable = 0,
                failed = 0,
                cancelled = 0,
                inFlight = 0,
                finished = false,
                checkpointSaved = false,
                ownerKnown = false
            )
        )

        assertEquals(setOf(ShaderPreparationUiAction.Cancel), actions)
        assertFalse(ShaderPreparationUiAction.ContinueToGame in actions)
    }
}
