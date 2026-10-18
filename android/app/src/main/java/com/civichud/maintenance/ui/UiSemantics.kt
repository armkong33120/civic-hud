package com.civichud.maintenance.ui

import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTagsAsResourceId

/** Makes the stable Compose test tags available to shell/UIAutomator tooling. */
@OptIn(ExperimentalComposeUiApi::class)
internal fun Modifier.automationRoot(): Modifier = semantics { testTagsAsResourceId = true }
