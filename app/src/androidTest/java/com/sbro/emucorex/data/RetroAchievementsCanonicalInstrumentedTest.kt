package com.sbro.emucorex.data

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import kotlinx.coroutines.runBlocking
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class RetroAchievementsCanonicalInstrumentedTest {
    private val preferences by lazy {
        // The instrumentation package has isolated storage; these tests never touch
        // a real EmuCoreX profile, token, library, or remembered password.
        AppPreferences(InstrumentationRegistry.getInstrumentation().context)
    }

    @Before
    fun clearTestCache() = runBlocking {
        preferences.setAchievementsProfileCache(null)
        preferences.setAchievementsAccountProgressCache(null)
    }

    @After
    fun cleanTestCache() = runBlocking {
        preferences.setAchievementsProfileCache(null)
        preferences.setAchievementsAccountProgressCache(null)
    }

    @Test
    fun progressPayload_acceptsCompleteAndTrulyEmptySnapshotsOnly() {
        assertTrue("""{"games":[]}""".isRetroAchievementAccountProgressPayload())
        assertTrue(completeProgressPayload().isRetroAchievementAccountProgressPayload())

        assertFalse("".isRetroAchievementAccountProgressPayload())
        assertFalse("not-json".isRetroAchievementAccountProgressPayload())
        assertFalse("""{"items":[]}""".isRetroAchievementAccountProgressPayload())
        assertFalse("""{"games":[{"gameId":12,"title":"","totalAchievements":5}]}""".isRetroAchievementAccountProgressPayload())
        assertFalse("""{"games":[{"gameId":0,"title":"Game","totalAchievements":5}]}""".isRetroAchievementAccountProgressPayload())
        assertFalse("""{"games":[{"gameId":12,"title":"Game","totalAchievements":-1}]}""".isRetroAchievementAccountProgressPayload())
        assertFalse("""{"games":[null]}""".isRetroAchievementAccountProgressPayload())
    }

    @Test
    fun progressParser_preservesIdentityNormalizesImagesAndBoundsCorruptCounters() {
        val games = completeProgressPayload().toRetroAchievementAccountProgress()

        assertEquals(2, games.size)
        assertEquals(12L, games[0].gameId)
        assertEquals("Game & One", games[0].title)
        assertEquals("https://media.retroachievements.org/Images/000012.png", games[0].gameImageUrl)
        assertEquals(8, games[0].totalAchievements)
        assertEquals(8, games[0].earnedAchievements)
        assertEquals(8, games[0].earnedHardcoreAchievements)
        assertEquals(0, games[1].earnedAchievements)
        assertEquals(0, games[1].earnedHardcoreAchievements)
    }

    @Test
    fun persistentCaches_roundTripAtomicallyAndClearEveryOwnershipField() = runBlocking {
        preferences.setAchievementsProfileCache(
            AchievementsProfileCache(
                username = "PlayerOne",
                displayName = "Player One",
                avatarPath = "/cache/avatar.png",
                points = -5,
                softcorePoints = 17,
                unreadMessages = -2,
                updatedAtMillis = 1234L
            )
        )
        preferences.setAchievementsAccountProgressCache(
            AchievementsAccountProgressCache(
                username = "PlayerOne",
                json = completeProgressPayload(),
                updatedAtMillis = 5678L
            )
        )

        val profile = preferences.getAchievementsProfileCacheSync()
        val progress = preferences.getAchievementsAccountProgressCache()
        assertEquals("PlayerOne", profile?.username)
        assertEquals("Player One", profile?.displayName)
        assertEquals(0, profile?.points)
        assertEquals(17, profile?.softcorePoints)
        assertEquals(0, profile?.unreadMessages)
        assertEquals("PlayerOne", progress?.username)
        assertEquals(5678L, progress?.updatedAtMillis)
        assertTrue(requireNotNull(progress).json.isRetroAchievementAccountProgressPayload())

        preferences.setAchievementsProfileCache(null)
        preferences.setAchievementsAccountProgressCache(null)
        assertNull(preferences.getAchievementsProfileCacheSync())
        assertNull(preferences.getAchievementsAccountProgressCache())
    }

    private fun completeProgressPayload(): String = """
        {
          "games": [
            {
              "gameId": 12,
              "title": "Game & One",
              "gameImageUrl": "/Images/000012.png",
              "earnedAchievements": 99,
              "earnedHardcoreAchievements": 55,
              "totalAchievements": 8
            },
            {
              "gameId": 13,
              "title": "Game Two",
              "gameImageUrl": "https://media.retroachievements.org/Images/000013.png",
              "earnedAchievements": -3,
              "earnedHardcoreAchievements": -1,
              "totalAchievements": 4
            }
          ]
        }
    """.trimIndent()
}
