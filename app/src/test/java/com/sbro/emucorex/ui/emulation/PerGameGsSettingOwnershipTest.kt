package com.sbro.emucorex.ui.emulation

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class PerGameGsSettingOwnershipTest {
    @Test
    fun absentChoiceInheritsGlobalOwnershipButExplicitNeutralChoiceIsOwned() {
        assertFalse(isPerGameGsSettingOwned(setOf("renderer"), "hwDownloadMode", false))
        assertTrue(isPerGameGsSettingOwned(setOf("hwDownloadMode"), "hwDownloadMode", false))
        assertTrue(isPerGameGsSettingOwned(setOf("blendingAccuracy"), "blendingAccuracy", false))
        assertTrue(isPerGameGsSettingOwned(null, "hwDownloadMode", false))
        assertTrue(isPerGameGsSettingOwned(setOf("renderer"), "blendingAccuracy", true))
    }

    @Test
    fun explicitNeutralGsChoicesRemainClaimedAfterRuntimePersistenceAndReload() {
        for (key in listOf("hwDownloadMode", "blendingAccuracy")) {
            val explicitProfileKeys = setOf(key)
            val appliedOwner = isPerGameGsSettingOwned(explicitProfileKeys, key, globalOwned = false)
            assertTrue("$key is explicitly claimed when applied", appliedOwner)

            // The runtime value equals the global neutral value, so value-diff key generation
            // produces no key. Persistence must retain the original profile claim.
            val persistedProfileKeys = mergePerGameGsProvidedKeys(
                runtimeKeys = emptySet(),
                existingKeys = explicitProfileKeys
            )
            assertTrue("$key remains serialized", key in requireNotNull(persistedProfileKeys))

            val reloadedOwner = isPerGameGsSettingOwned(
                persistedProfileKeys,
                key,
                globalOwned = false
            )
            assertTrue("$key remains claimed after reload", reloadedOwner)
        }
    }

    @Test
    fun explicitNeutralGsChoiceMadeDuringRuntimeIsPersistedEvenWhenValueMatchesGlobal() {
        for (key in listOf("hwDownloadMode", "blendingAccuracy")) {
            val persistedProfileKeys = mergePerGameGsProvidedKeys(
                runtimeKeys = emptySet(),
                existingKeys = emptySet(),
                explicitlyClaimedGsKeys = setOf(key)
            )
            assertTrue("$key explicit runtime choice is serialized", key in requireNotNull(persistedProfileKeys))
            assertTrue(isPerGameGsSettingOwned(persistedProfileKeys, key, globalOwned = false))
        }
    }

    @Test
    fun inheritedGlobalGsChoicesAreNotPinnedIntoPerGameProfile() {
        val persistedProfileKeys = requireNotNull(
            mergePerGameGsProvidedKeys(runtimeKeys = emptySet(), existingKeys = emptySet())
        )
        assertFalse("hwDownloadMode stays inherited", "hwDownloadMode" in persistedProfileKeys)
        assertFalse("blendingAccuracy stays inherited", "blendingAccuracy" in persistedProfileKeys)
    }
    @Test
    fun legacyAllProvidedProfileRetainsNeutralGsClaimsWhenSaved() {
        val keys = requireNotNull(mergePerGameGsProvidedKeys(emptySet(), existingKeys = null))
        assertTrue("Legacy HW mode remains owned", "hwDownloadMode" in keys)
        assertTrue("Legacy blending remains owned", "blendingAccuracy" in keys)
    }
}
