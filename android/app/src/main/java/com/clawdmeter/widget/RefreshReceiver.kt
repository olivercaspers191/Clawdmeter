package com.clawdmeter.widget

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

/**
 * Handles the tap-to-refresh broadcast.
 *
 * Separate from UsageWidgetProvider on purpose. The widget provider *must* be
 * exported so the system can deliver APPWIDGET_UPDATE to it — but that would
 * also let any other app on the phone fire our custom REFRESH action and drive a
 * network fetch (no data leaks, but it's a free battery/data-drain trigger).
 *
 * This receiver has no intent-filter, so it defaults to exported=false: only our
 * own PendingIntent, which names it by class, can reach it.
 */
class RefreshReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action == ACTION_REFRESH) {
            RefreshWorker.refreshNow(context.applicationContext)
        }
    }

    companion object {
        const val ACTION_REFRESH = "com.clawdmeter.widget.REFRESH"
    }
}
