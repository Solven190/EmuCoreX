package com.sbro.emucorex.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class GameRepositoryTest {

    @Test
    fun parentDocumentId_returnsContainingFolderForNestedDocument() {
        assertEquals(
            "primary:Games PS2",
            GameRepository.parentDocumentId("primary:Games PS2/007 - Nightfire.iso")
        )
    }

    @Test
    fun parentDocumentId_returnsVolumeRootForDocumentAtRoot() {
        assertEquals("primary:", GameRepository.parentDocumentId("primary:game.iso"))
    }

    @Test
    fun relativeDocumentSegments_returnsPathBelowPersistedTree() {
        assertEquals(
            listOf("Action", "Disc 1"),
            GameRepository.relativeDocumentSegments(
                rootDocumentId = "primary:Games PS2",
                targetDocumentId = "primary:Games PS2/Action/Disc 1"
            )
        )
    }

    @Test
    fun relativeDocumentSegments_supportsVolumeRootTree() {
        assertEquals(
            listOf("Games PS2"),
            GameRepository.relativeDocumentSegments(
                rootDocumentId = "primary:",
                targetDocumentId = "primary:Games PS2"
            )
        )
    }

    @Test
    fun relativeDocumentSegments_rejectsUnrelatedTree() {
        assertNull(
            GameRepository.relativeDocumentSegments(
                rootDocumentId = "primary:Other",
                targetDocumentId = "primary:Games PS2"
            )
        )
    }
}
