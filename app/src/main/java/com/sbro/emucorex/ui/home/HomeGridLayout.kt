package com.sbro.emucorex.ui.home

internal fun calculateHomeGridColumnCount(
    screenWidthDp: Int,
    screenHeightDp: Int,
    smallestScreenWidthDp: Int,
    gridScale: Float
): Int {
    val isLandscape = screenWidthDp > screenHeightDp
    val isWide = smallestScreenWidthDp >= 600 && screenWidthDp >= 900
    val contentWidthDp = if (isWide) (screenWidthDp - 332).coerceAtLeast(320) else screenWidthDp
    val baseCellSizeDp = if (isLandscape) 94 else 102
    val minCellSizeDp = (baseCellSizeDp * gridScale).toInt().coerceAtLeast(1)
    return maxOf(1, (contentWidthDp + 12) / (minCellSizeDp + 12))
}
