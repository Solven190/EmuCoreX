package com.sbro.emucorex.data

import android.content.Context
import android.content.ContextWrapper
import android.content.SharedPreferences
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.PreferenceDataStoreFactory
import androidx.datastore.preferences.core.Preferences
import com.sbro.emucorex.core.GsHackDefaults
import com.sbro.emucorex.ui.settings.SettingsUiState
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.runBlocking
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.After
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

@RunWith(AndroidJUnit4::class)
class GameDbFixDefaultsInstrumentedTest {
    private val base = ApplicationProvider.getApplicationContext<Context>()
    private val id = "gamedb-defaults-${System.nanoTime()}"
    private val root = File(base.cacheDir, id).apply { mkdirs() }
    // Context wrappers isolate locale/profile files, but preferencesDataStore
    // caches one INSTANCE across all contexts. Replace that instance for this test.
    private val context = object : ContextWrapper(base) {
        override fun getApplicationContext(): Context = this
        override fun getFilesDir(): File = File(root, "files").apply { mkdirs() }
        override fun getExternalFilesDir(type: String?): File = File(root, "external").apply { mkdirs() }
        override fun getSharedPreferences(name: String, mode: Int): SharedPreferences =
            base.getSharedPreferences("$id-$name", mode)
    }
    private val preferences by lazy { AppPreferences(context) }
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private lateinit var delegate: Any
    private lateinit var instanceField: java.lang.reflect.Field
    private var originalStore: Any? = null
    private var originalPreferences: Preferences? = null

    @Before
    fun isolateDataStore() = runBlocking {
        val field = Class.forName("com.sbro.emucorex.data.AppPreferencesKt")
            .getDeclaredField("dataStore\$delegate").apply { isAccessible = true }
        delegate = field.get(null)!!
        instanceField = delegate.javaClass.getDeclaredField("INSTANCE").apply { isAccessible = true }
        originalStore = instanceField.get(delegate)
        @Suppress("UNCHECKED_CAST")
        originalPreferences = (originalStore as? DataStore<Preferences>)?.data?.first()
        instanceField.set(delegate, PreferenceDataStoreFactory.create(scope = scope) {
            File(context.filesDir, "settings.preferences_pb")
        })
    }

    @After
    fun restoreDataStore() = runBlocking {
        if (::instanceField.isInitialized) {
            instanceField.set(delegate, originalStore)
            scope.cancel()
            @Suppress("UNCHECKED_CAST")
            val restored = (originalStore as? DataStore<Preferences>)?.data?.first()
            assertEquals("User preferences must remain untouched", originalPreferences, restored)
        }
    }

    @Test
    fun freshSettingsAndExportEnableGraphicsAndKeepCoreFixes() = runBlocking {
        assertTrue(AppPreferences.DEFAULT_AUTO_GS_HARDWARE_FIXES)
        assertTrue(SettingsSnapshot().autoGsHardwareFixes)
        assertTrue(SettingsUiState().autoGsHardwareFixes)
        assertTrue(PerGameSettings("test", "Test").autoGsHardwareFixes)
        assertTrue(preferences.autoGsHardwareFixes.first())
        val settings = preferences.settingsSnapshot.first()
        assertTrue(settings.autoGsHardwareFixes)
        assertFalse(settings.hwDownloadModeUserOverride)
        assertFalse(settings.blendingAccuracyUserOverride)
        assertTrue(settings.enableGameFixes)
        assertEquals(AppPreferences.DEFAULT_EE_FPU_CLAMPING_MODE, settings.eeFpuClampingMode)
        val backup = preferences.exportJson()
        assertTrue(backup.getBoolean("autoGsHardwareFixes"))
        assertTrue(backup.getBoolean("enableGameFixes"))
    }

    @Test
    fun legacyBackupAndResetUseGraphicsDefaultWithoutChangingClampOverrides() = runBlocking {
        preferences.importJson(JSONObject().put("eeFpuClampingMode", 3).put("vu0ClampingMode", 2))
        assertTrue(preferences.autoGsHardwareFixes.first())
        val imported = preferences.settingsSnapshot.first()
        assertTrue(imported.enableGameFixes)
        assertFalse(imported.hwDownloadModeUserOverride)
        assertFalse(imported.blendingAccuracyUserOverride)
        assertEquals(3, imported.eeFpuClampingMode)
        assertEquals(2, imported.vu0ClampingMode)
        preferences.setAutoGsHardwareFixes(true)
        preferences.resetAllSettings()
        assertTrue(preferences.autoGsHardwareFixes.first())
        assertTrue(preferences.settingsSnapshot.first().enableGameFixes)
    }

    @Test
    fun explicitGlobalChoicesSurviveBackupRoundTrip() = runBlocking {
        for (enabled in listOf(true, false)) {
            preferences.setAutoGsHardwareFixes(enabled)
            preferences.setEnableGameFixes(true)
            val backup = preferences.exportJson()
            preferences.resetAllSettings()
            preferences.importJson(backup)
            assertEquals(enabled, preferences.autoGsHardwareFixes.first())
            assertEquals(enabled, preferences.settingsSnapshot.first().autoGsHardwareFixes)
            assertTrue(preferences.settingsSnapshot.first().enableGameFixes)
        }
    }

    @Test
    fun gsModeOwnershipDistinguishesAbsentDefaultsFromExplicitNeutralValues() = runBlocking {
        val fresh = preferences.settingsSnapshot.first()
        assertFalse(fresh.hwDownloadModeUserOverride)
        assertFalse(fresh.blendingAccuracyUserOverride)

        val freshBackup = preferences.exportJson()
        assertFalse(freshBackup.getBoolean("hwDownloadModeUserOverride"))
        assertFalse(freshBackup.getBoolean("blendingAccuracyUserOverride"))
        preferences.importJson(freshBackup)
        val restoredDefaults = preferences.settingsSnapshot.first()
        assertFalse(restoredDefaults.hwDownloadModeUserOverride)
        assertFalse(restoredDefaults.blendingAccuracyUserOverride)

        preferences.setHwDownloadMode(0)
        preferences.setBlendingAccuracy(2)
        val setterChoices = preferences.settingsSnapshot.first()
        assertTrue(setterChoices.hwDownloadModeUserOverride)
        assertTrue(setterChoices.blendingAccuracyUserOverride)

        preferences.importJson(freshBackup)
        val importedDefaults = preferences.settingsSnapshot.first()
        assertFalse(importedDefaults.hwDownloadModeUserOverride)
        assertFalse(importedDefaults.blendingAccuracyUserOverride)

        preferences.importJson(
            JSONObject()
                .put("hwDownloadMode", 0)
                .put("blendingAccuracy", 2)
        )
        val legacyExplicitValues = preferences.settingsSnapshot.first()
        assertTrue(legacyExplicitValues.hwDownloadModeUserOverride)
        assertTrue(legacyExplicitValues.blendingAccuracyUserOverride)
        assertEquals(0, legacyExplicitValues.hwDownloadMode)
        assertEquals(2, legacyExplicitValues.blendingAccuracy)

        val explicitBackup = preferences.exportJson()
        preferences.resetAllSettings()
        preferences.importJson(explicitBackup)
        val restoredExplicit = preferences.settingsSnapshot.first()
        assertTrue(restoredExplicit.hwDownloadModeUserOverride)
        assertTrue(restoredExplicit.blendingAccuracyUserOverride)

        preferences.resetAllSettings()
        val reset = preferences.settingsSnapshot.first()
        assertFalse(reset.hwDownloadModeUserOverride)
        assertFalse(reset.blendingAccuracyUserOverride)
        assertEquals(0, reset.hwDownloadMode)
        assertEquals(GsHackDefaults.BLENDING_ACCURACY_DEFAULT, reset.blendingAccuracy)
    }

    @Test
    fun legacyAndExplicitGameProfilesPreserveIndependentCoreSettings() {
        val repository = PerGameSettingsRepository(context)
        val legacy = JSONObject().put("gameKey", "legacy").put("gameTitle", "Legacy")
            .put("eeFpuClampingMode", 3)
        repository.importJson(JSONObject().put("profiles", JSONArray().put(legacy)))
        val loaded = requireNotNull(repository.get("legacy"))
        assertTrue(loaded.autoGsHardwareFixes)
        assertTrue(loaded.enableGameFixes)
        assertEquals(3, loaded.eeFpuClampingMode)
        for (enabled in listOf(true, false)) {
            repository.save(loaded.copy(autoGsHardwareFixes = enabled, providedKeys = null))
            val backup = repository.exportJson()
            repository.deleteAll()
            repository.importJson(backup)
            val restored = requireNotNull(repository.get("legacy"))
            assertEquals(enabled, restored.autoGsHardwareFixes)
            assertTrue(restored.enableGameFixes)
            assertEquals(3, restored.eeFpuClampingMode)
        }
    }
}
