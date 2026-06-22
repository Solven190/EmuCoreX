package com.sbro.emucorex.ui.common

import android.view.WindowManager
import android.view.Gravity
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Build
import android.view.View
import android.view.WindowInsets
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Info
import androidx.compose.material.icons.rounded.Restore
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.compose.ui.window.DialogWindowProvider
import com.sbro.emucorex.R

@Composable
fun SettingHelpButton(
    title: String,
    description: String,
    modifier: Modifier = Modifier
) {
    val showDialog = remember(title, description) { mutableStateOf(false) }
    val interactionSource = remember { MutableInteractionSource() }

    Box(
        modifier = modifier
            .size(18.dp)
            .clickable(
                interactionSource = interactionSource,
                indication = null,
                onClick = { showDialog.value = true }
            ),
        contentAlignment = Alignment.Center
    ) {
        Icon(
            imageVector = Icons.Rounded.Info,
            contentDescription = stringResource(R.string.settings_help_content_description),
            tint = MaterialTheme.colorScheme.primary.copy(alpha = 0.92f),
            modifier = Modifier.size(14.dp)
        )
    }

    if (showDialog.value) {
        val configuration = LocalConfiguration.current
        val isLandscape = configuration.screenWidthDp > configuration.screenHeightDp
        val maxDialogHeight = if (isLandscape) {
            (configuration.screenHeightDp.dp - 40.dp).coerceAtLeast(250.dp)
        } else {
            (configuration.screenHeightDp.dp - 64.dp).coerceAtLeast(540.dp)
        }
        val dialogWidthFraction = if (isLandscape) 0.98f else 0.94f
        val dialogMaxWidth = if (isLandscape) 1600.dp else 720.dp
        val contentHorizontalPadding = if (isLandscape) 20.dp else 22.dp
        val contentTopPadding = if (isLandscape) 18.dp else 22.dp
        val contentBottomPadding = if (isLandscape) 24.dp else 28.dp
        val contentSpacing = if (isLandscape) 14.dp else 18.dp
        val iconTileSize = if (isLandscape) 52.dp else 58.dp
        val iconSize = if (isLandscape) 28.dp else 30.dp

        Dialog(
            onDismissRequest = { showDialog.value = false },
            properties = DialogProperties(
                usePlatformDefaultWidth = false,
                decorFitsSystemWindows = false
            )
        ) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(
                        start = if (isLandscape) 10.dp else 14.dp,
                        top = if (isLandscape) 10.dp else 14.dp,
                        end = if (isLandscape) 10.dp else 14.dp,
                        bottom = if (isLandscape) 4.dp else 8.dp
                    ),
                contentAlignment = Alignment.Center
            ) {
                Surface(
                    modifier = Modifier
                        .fillMaxWidth(dialogWidthFraction)
                        .widthIn(max = dialogMaxWidth),
                    shape = RoundedCornerShape(30.dp),
                    color = MaterialTheme.colorScheme.surface
                ) {
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .heightIn(max = maxDialogHeight)
                            .verticalScroll(rememberScrollState())
                            .padding(
                                start = contentHorizontalPadding,
                                top = contentTopPadding,
                                end = contentHorizontalPadding,
                                bottom = contentBottomPadding
                            ),
                        verticalArrangement = Arrangement.spacedBy(contentSpacing)
                    ) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Surface(
                                modifier = Modifier.size(iconTileSize),
                                shape = RoundedCornerShape(18.dp),
                                color = MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.78f)
                            ) {
                                Box(contentAlignment = Alignment.Center) {
                                    Icon(
                                        imageVector = Icons.Rounded.Info,
                                        contentDescription = null,
                                        tint = MaterialTheme.colorScheme.primary,
                                        modifier = Modifier.size(iconSize)
                                    )
                                }
                            }
                            Spacer(modifier = Modifier.width(14.dp))
                            Column(
                                modifier = Modifier.weight(1f),
                                verticalArrangement = Arrangement.spacedBy(2.dp)
                            ) {
                                Text(
                                    text = stringResource(R.string.settings_help_dialog_eyebrow),
                                    style = MaterialTheme.typography.titleSmall,
                                    fontWeight = FontWeight.SemiBold,
                                    color = MaterialTheme.colorScheme.primary
                                )
                                Text(
                                    text = title,
                                    style = MaterialTheme.typography.headlineSmall,
                                    fontWeight = FontWeight.Bold,
                                    color = MaterialTheme.colorScheme.onSurface
                                )
                            }
                        }

                        HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.72f))

                        Text(
                            text = description,
                            style = MaterialTheme.typography.bodyLarge,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )

                        Surface(
                            modifier = Modifier.fillMaxWidth(),
                            shape = RoundedCornerShape(18.dp),
                            color = MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.26f),
                            border = BorderStroke(
                                1.dp,
                                MaterialTheme.colorScheme.primary.copy(alpha = 0.22f)
                            )
                        ) {
                            Row(
                                modifier = Modifier.padding(horizontal = 14.dp, vertical = 13.dp),
                                verticalAlignment = Alignment.CenterVertically,
                                horizontalArrangement = Arrangement.spacedBy(12.dp)
                            ) {
                                Icon(
                                    imageVector = Icons.Rounded.Restore,
                                    contentDescription = null,
                                    tint = MaterialTheme.colorScheme.primary,
                                    modifier = Modifier.size(22.dp)
                                )
                                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                                    Text(
                                        text = stringResource(R.string.settings_help_reset_tip_title),
                                        style = MaterialTheme.typography.titleSmall,
                                        fontWeight = FontWeight.SemiBold,
                                        color = MaterialTheme.colorScheme.onSurface
                                    )
                                    Text(
                                        text = stringResource(R.string.settings_help_reset_tip_body),
                                        style = MaterialTheme.typography.bodySmall,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant
                                    )
                                }
                            }
                        }

                        Button(
                            onClick = { showDialog.value = false },
                            modifier = Modifier.fillMaxWidth(),
                            shape = RoundedCornerShape(18.dp),
                            colors = ButtonDefaults.buttonColors(
                                containerColor = MaterialTheme.colorScheme.primary,
                                contentColor = MaterialTheme.colorScheme.onPrimary
                            )
                        ) {
                            Text(
                                text = stringResource(R.string.close),
                                style = MaterialTheme.typography.titleMedium,
                                fontWeight = FontWeight.SemiBold,
                                modifier = Modifier.padding(vertical = 6.dp)
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun DialogWindowWidth(
    enabled: Boolean,
    widthFraction: Float,
    maxWidthDp: Int
) {
    val view = LocalView.current
    val density = LocalDensity.current
    val configuration = LocalConfiguration.current

    SideEffect {
        val window = (view.parent as? DialogWindowProvider)?.window ?: return@SideEffect
        window.setGravity(Gravity.CENTER)
        window.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
        window.decorView.setPadding(0, 0, 0, 0)
        if (!enabled) {
            val attributes = window.attributes
            attributes.x = 0
            window.attributes = attributes
            window.setLayout(
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT
            )
            return@SideEffect
        }

        val requestedWidthPx = with(density) {
            (configuration.screenWidthDp.dp * widthFraction)
                .coerceAtMost(maxWidthDp.dp)
                .roundToPx()
        }
        val attributes = window.attributes
        attributes.x = landscapeCenterOffsetPx(view, configuration.screenWidthDp > configuration.screenHeightDp)
        window.attributes = attributes
        window.setLayout(requestedWidthPx, WindowManager.LayoutParams.WRAP_CONTENT)
    }
}

private fun landscapeCenterOffsetPx(view: View, isLandscape: Boolean): Int {
    if (!isLandscape) return 0
    val insets = view.rootWindowInsets ?: return 0
    val leftRight = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
        val bars = insets.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout())
        bars.left to bars.right
    } else {
        @Suppress("DEPRECATION")
        insets.systemWindowInsetLeft to insets.systemWindowInsetRight
    }
    return (leftRight.first - leftRight.second) / 2
}
