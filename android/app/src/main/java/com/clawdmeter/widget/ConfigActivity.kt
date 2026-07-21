package com.clawdmeter.widget

import android.app.Activity
import android.appwidget.AppWidgetManager
import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import kotlin.concurrent.thread

/**
 * Widget setup: where is the usage service?
 *
 * Deliberately not hardcoded to a default host — the URL is the user's own LAN
 * or tailnet address, and baking someone's network layout into a public repo is
 * exactly the kind of thing that shouldn't be in source.
 */
class ConfigActivity : Activity() {

    private var widgetId = AppWidgetManager.INVALID_APPWIDGET_ID

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Cancelled-result first: if the user backs out, the launcher must not
        // leave a half-placed widget on the home screen.
        setResult(RESULT_CANCELED)

        widgetId = intent?.extras?.getInt(
            AppWidgetManager.EXTRA_APPWIDGET_ID, AppWidgetManager.INVALID_APPWIDGET_ID
        ) ?: AppWidgetManager.INVALID_APPWIDGET_ID

        setContentView(R.layout.activity_config)

        val input = findViewById<EditText>(R.id.url_input)
        val status = findViewById<TextView>(R.id.status)
        input.setText(UsageRepository.baseUrl(this))

        findViewById<Button>(R.id.save).setOnClickListener {
            var url = input.text.toString().trim().trimEnd('/')
            if (url.isEmpty()) {
                Toast.makeText(this, "Enter the server address", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            if (!url.startsWith("http://") && !url.startsWith("https://")) url = "http://$url"

            UsageRepository.setBaseUrl(this, url)
            status.text = "Checking…"

            // Verify before finishing, so a typo surfaces here rather than as a
            // permanently sad widget on the home screen.
            thread {
                val data = UsageRepository.refresh(this)
                val err = UsageRepository.lastError(this)
                runOnUiThread {
                    if (data != null && err == null) {
                        UsageWidgetProvider.updateAll(this, data)
                        RefreshWorker.ensureScheduled(this)
                        finishOk()
                    } else {
                        status.text = "Couldn't reach it: ${err ?: "unknown error"}"
                    }
                }
            }
        }

        findViewById<Button>(R.id.skip).setOnClickListener {
            // Save anyway and let the widget show its error state — useful when
            // setting up away from the tailnet.
            UsageRepository.setBaseUrl(this, input.text.toString().trim().trimEnd('/'))
            UsageWidgetProvider.updateAll(this, UsageRepository.cached(this))
            RefreshWorker.ensureScheduled(this)
            finishOk()
        }
    }

    private fun finishOk() {
        if (widgetId != AppWidgetManager.INVALID_APPWIDGET_ID) {
            setResult(
                RESULT_OK,
                Intent().putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, widgetId),
            )
        } else {
            setResult(RESULT_OK)
        }
        finish()
    }
}
