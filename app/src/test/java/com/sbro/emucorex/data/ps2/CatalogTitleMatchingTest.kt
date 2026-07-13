package com.sbro.emucorex.data.ps2

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class CatalogTitleMatchingTest {

    @Test
    fun `normalization follows catalog title keys`() {
        assertEquals(
            "baldur s gate dark alliance ii",
            normalizeIdentityTitle("Baldur's Gate: Dark Alliance II")
        )
    }

    @Test
    fun `roman and numeric sequels match`() {
        assertEquals(1.0, catalogTitleSimilarity("Tekken 5", "Tekken V"), 0.0)
    }

    @Test
    fun `short franchise title matches expanded catalog title`() {
        assertTrue(catalogTitleSimilarity("007 Nightfire", "James Bond 007 Nightfire") >= 0.68)
    }

    @Test
    fun `character prefix does not prevent tomb raider match`() {
        assertTrue(
            catalogTitleSimilarity(
                "Lara Croft Tomb Raider Anniversary",
                "Tomb Raider Anniversary"
            ) >= 0.68
        )
    }

    @Test
    fun `unrelated games do not match`() {
        assertTrue(catalogTitleSimilarity("Tekken 5", "Gran Turismo 4") < 0.68)
    }
}
