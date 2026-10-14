package com.sbro.emucorex.data

import android.content.Context
import android.content.ContextWrapper
import android.content.SharedPreferences
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.PreferenceDataStoreFactory
import androidx.datastore.preferences.core.Preferences
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
    fun freshSettingsAndExportDisableGraphicsButKeepCoreFixes() = runBlocking {
        assertFalse(SettingsSnapshot().autoGsHardwareFixes)
        assertFalse(SettingsUiState().autoGsHardwareFixes)
        assertFalse(PerGameSettings("test", "Test").autoGsHardwareFixes)
        assertFalse(preferences.autoGsHardwareFixes.first())
        val settings = preferences.settingsSnapshot.first()
        assertFalse(settings.autoGsHardwareFixes)
        assertTrue(settings.enableGameFixes)
        assertEquals(AppPreferences.DEFAULT_EE_FPU_CLAMPING_MODE, settings.eeFpuClampingMode)
        val backup = preferences.exportJson()
        assertFalse(backup.getBoolean("autoGsHardwareFixes"))
        assertTrue(backup.getBoolean("enableGameFixes"))
    }

    @Test
    fun legacyBackupAndResetUseGraphicsDefaultWithoutChangingClampOverrides() = runBlocking {
        preferences.importJson(JSONObject().put("eeFpuClampingMode", 3).put("vu0ClampingMode", 2))
        assertFalse(preferences.autoGsHardwareFixes.first())
        val imported = preferences.settingsSnapshot.first()
        assertTrue(imported.enableGameFixes)
        assertEquals(3, imported.eeFpuClampingMode)
        assertEquals(2, imported.vu0ClampingMode)
        preferences.setAutoGsHardwareFixes(true)
        preferences.resetAllSettings()
        assertFalse(preferences.autoGsHardwareFixes.first())
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
    fun legacyAndExplicitGameProfilesPreserveIndependentCoreSettings() {
        val repository = PerGameSettingsRepository(context)
        val legacy = JSONObject().put("gameKey", "legacy").put("gameTitle", "Legacy")
            .put("eeFpuClampingMode", 3)
        repository.importJson(JSONObject().put("profiles", JSONArray().put(legacy)))
        val loaded = requireNotNull(repository.get("legacy"))
        assertFalse(loaded.autoGsHardwareFixes)
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
