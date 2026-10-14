package com.sbro.emucorex.ui.emulation

internal data class TouchButtonPointerTransition(
    val enteredTargets: Set<String> = emptySet(),
    val exitedTargets: Set<String> = emptySet(),
    val activatedTargets: Set<String> = emptySet(),
    val deactivatedTargets: Set<String> = emptySet(),
    val pressedActionOwners: Set<String> = emptySet(),
    val releasedActionOwners: Set<String> = emptySet(),
    val activeTargets: Set<String> = emptySet()
)

/** Tracks pointer ownership separately from the logical gamepad actions those targets drive. */
internal class TouchButtonPointerState(
    private val actionOwnersByTarget: Map<String, Set<String>>,
    private val pointerActionTargets: Set<String> = actionOwnersByTarget.keys
) {
    private val targetsByPointer = mutableMapOf<Long, Set<String>>()
    private val latchedTargets = mutableSetOf<String>()
    private val transientOwnersBySource = mutableMapOf<String, Set<String>>()

    fun update(pointerId: Long, targets: Collection<String>): TouchButtonPointerTransition {
        val oldTargets = targetsByPointer[pointerId].orEmpty()
        val newTargets = targets.toCollection(LinkedHashSet())
        val previouslyActiveTargets = targetsByPointer.values.flatten().toSet()
        val previouslyActiveOwners = activeActionOwners()

        if (newTargets.isEmpty()) {
            targetsByPointer.remove(pointerId)
        } else {
            targetsByPointer[pointerId] = newTargets
        }

        val activeTargets = targetsByPointer.values.flatten().toCollection(LinkedHashSet())
        val activeOwners = activeActionOwners()
        return TouchButtonPointerTransition(
            enteredTargets = newTargets - oldTargets,
            exitedTargets = oldTargets - newTargets,
            activatedTargets = activeTargets - previouslyActiveTargets,
            deactivatedTargets = previouslyActiveTargets - activeTargets,
            pressedActionOwners = activeOwners - previouslyActiveOwners,
            releasedActionOwners = previouslyActiveOwners - activeOwners,
            activeTargets = activeTargets
        )
    }

    fun completePointer(
        pointerId: Long,
        latchedUpdates: Map<String, Boolean> = emptyMap(),
        transientActionOwners: Map<String, Set<String>> = emptyMap()
    ): TouchButtonPointerTransition {
        val oldTargets = targetsByPointer[pointerId].orEmpty()
        val previouslyActiveTargets = targetsByPointer.values.flatten().toSet()
        val previouslyActiveOwners = activeActionOwners()
        targetsByPointer.remove(pointerId)
        latchedUpdates.forEach { (targetId, latched) ->
            if (latched) latchedTargets.add(targetId) else latchedTargets.remove(targetId)
        }
        transientActionOwners.forEach { (sourceId, owners) ->
            if (owners.isEmpty()) transientOwnersBySource.remove(sourceId) else {
                transientOwnersBySource[sourceId] = owners.toSet()
            }
        }
        val activeTargets = targetsByPointer.values.flatten().toCollection(LinkedHashSet())
        val activeOwners = activeActionOwners()
        return TouchButtonPointerTransition(
            exitedTargets = oldTargets,
            deactivatedTargets = previouslyActiveTargets - activeTargets,
            pressedActionOwners = activeOwners - previouslyActiveOwners,
            releasedActionOwners = previouslyActiveOwners - activeOwners,
            activeTargets = activeTargets
        )
    }

    fun activeTargets(): Set<String> =
        targetsByPointer.values.flatten().toCollection(LinkedHashSet())

    fun setLatched(targetId: String, latched: Boolean): TouchButtonPointerTransition {
        val previouslyActiveOwners = activeActionOwners()
        if (latched) latchedTargets.add(targetId) else latchedTargets.remove(targetId)
        val activeOwners = activeActionOwners()
        return TouchButtonPointerTransition(
            pressedActionOwners = activeOwners - previouslyActiveOwners,
            releasedActionOwners = previouslyActiveOwners - activeOwners,
            activeTargets = activeTargets()
        )
    }

    fun setTransientActionOwners(
        sourceId: String,
        owners: Set<String>
    ): TouchButtonPointerTransition {
        val previouslyActiveOwners = activeActionOwners()
        if (owners.isEmpty()) transientOwnersBySource.remove(sourceId) else {
            transientOwnersBySource[sourceId] = owners.toSet()
        }
        val activeOwners = activeActionOwners()
        return TouchButtonPointerTransition(
            pressedActionOwners = activeOwners - previouslyActiveOwners,
            releasedActionOwners = previouslyActiveOwners - activeOwners,
            activeTargets = activeTargets()
        )
    }

    private fun actionOwners(targets: Set<String>): Set<String> =
        targets.flatMapTo(LinkedHashSet()) { actionOwnersByTarget[it].orEmpty() }

    private fun activeActionOwners(): Set<String> = actionOwners(
        targetsByPointer.values.flatten().filterTo(LinkedHashSet(), pointerActionTargets::contains)
    ) + actionOwners(latchedTargets) + transientOwnersBySource.values.flatten().toSet()
}

internal data class TouchButtonHitCandidate(
    val id: String,
    val clickOnlyUtility: Boolean
)

/** Keeps utility clicks topmost-exclusive while allowing overlapping gameplay buttons together. */
internal fun selectTouchButtonHitTargets(
    candidatesInDrawOrder: List<TouchButtonHitCandidate>
): List<String> {
    val topmost = candidatesInDrawOrder.lastOrNull() ?: return emptyList()
    if (topmost.clickOnlyUtility) return listOf(topmost.id)
    return candidatesInDrawOrder.filterNot(TouchButtonHitCandidate::clickOnlyUtility).map(TouchButtonHitCandidate::id)
}

internal fun matchingTouchButtonReleaseTargets(
    downTargets: Set<String>,
    upTargets: Set<String>
): Set<String> = downTargets intersect upTargets
