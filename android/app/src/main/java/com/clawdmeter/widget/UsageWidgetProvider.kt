package com.clawdmeter.widget

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.widget.RemoteViews

class UsageWidgetProvider : AppWidgetProvider() {

    override fun onUpdate(ctx: Context, mgr: AppWidgetManager, ids: IntArray) {
        // Paint immediately from cache so the widget never shows a blank frame,
        // then go to the network off the main thread.
        renderAll(ctx, mgr, ids, UsageRepository.cached(ctx))
        RefreshWorker.refreshNow(ctx)
        RefreshWorker.ensureScheduled(ctx)
    }

    /** Fired on resize. The bitmap is size-specific, so it must be redrawn. */
    override fun onAppWidgetOptionsChanged(
        ctx: Context, mgr: AppWidgetManager, id: Int, newOptions: Bundle,
    ) {
        renderAll(ctx, mgr, intArrayOf(id), UsageRepository.cached(ctx))
    }

    override fun onReceive(ctx: Context, intent: Intent) {
        super.onReceive(ctx, intent)
        if (intent.action == ACTION_REFRESH) RefreshWorker.refreshNow(ctx)
    }

    override fun onDisabled(ctx: Context) {
        // Last widget removed — stop the periodic work rather than polling for
        // a widget that no longer exists.
        RefreshWorker.cancel(ctx)
    }

    companion object {
        const val ACTION_REFRESH = "com.clawdmeter.widget.REFRESH"

        fun renderAll(ctx: Context, mgr: AppWidgetManager, ids: IntArray, data: UsageData?) {
            val error = if (UsageRepository.baseUrl(ctx).isEmpty()) {
                "Tap to set up"
            } else {
                UsageRepository.lastError(ctx)?.let { "Can't reach server" }
            }

            for (id in ids) {
                val opts = mgr.getAppWidgetOptions(id)
                // MIN_* is the size guaranteed in the current orientation; using
                // it means content never overflows when the device rotates.
                val wDp = opts.getInt(AppWidgetManager.OPTION_APPWIDGET_MIN_WIDTH, 180)
                val hDp = opts.getInt(AppWidgetManager.OPTION_APPWIDGET_MIN_HEIGHT, 180)

                val views = RemoteViews(ctx.packageName, R.layout.widget)
                views.setImageViewBitmap(
                    R.id.widget_image,
                    WidgetRenderer.render(ctx, wDp, hDp, data, error),
                )
                views.setOnClickPendingIntent(R.id.widget_image, clickIntent(ctx, data))
                mgr.updateAppWidget(id, views)
            }
        }

        /** Tap refreshes once configured; before that it opens the setup screen. */
        private fun clickIntent(ctx: Context, data: UsageData?): PendingIntent {
            val flags = PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
            return if (UsageRepository.baseUrl(ctx).isEmpty()) {
                PendingIntent.getActivity(
                    ctx, 0,
                    Intent(ctx, ConfigActivity::class.java)
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
                    flags,
                )
            } else {
                PendingIntent.getBroadcast(
                    ctx, 0,
                    Intent(ctx, UsageWidgetProvider::class.java).setAction(ACTION_REFRESH),
                    flags,
                )
            }
        }

        /** Redraw every placed widget — called by the worker after a fetch. */
        fun updateAll(ctx: Context, data: UsageData?) {
            val mgr = AppWidgetManager.getInstance(ctx)
            val ids = mgr.getAppWidgetIds(
                android.content.ComponentName(ctx, UsageWidgetProvider::class.java)
            )
            if (ids.isNotEmpty()) renderAll(ctx, mgr, ids, data)
        }
    }
}
