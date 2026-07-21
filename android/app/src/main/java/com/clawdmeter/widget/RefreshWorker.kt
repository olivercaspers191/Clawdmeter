package com.clawdmeter.widget

import android.content.Context
import androidx.work.Constraints
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.ExistingWorkPolicy
import androidx.work.NetworkType
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.Worker
import androidx.work.WorkerParameters
import java.util.concurrent.TimeUnit

/**
 * Periodic + on-demand refresh.
 *
 * 15 minutes is not a choice — it is WorkManager's hard floor for periodic work.
 * That's fine here: the 5-hour and 7-day windows this tracks move slowly, and a
 * tap refreshes on demand when you want the current number.
 */
class RefreshWorker(ctx: Context, params: WorkerParameters) : Worker(ctx, params) {

    override fun doWork(): Result {
        val data = UsageRepository.refresh(applicationContext)
        UsageWidgetProvider.updateAll(applicationContext, data)
        // Always success: a failed fetch already fell back to cache and the
        // widget shows the age, so retrying on WorkManager's backoff would just
        // burn battery for data that the next tick will pick up anyway.
        return Result.success()
    }

    companion object {
        private const val PERIODIC = "clawdmeter-refresh"
        private const val ONESHOT = "clawdmeter-refresh-now"

        fun ensureScheduled(ctx: Context) {
            val req = PeriodicWorkRequestBuilder<RefreshWorker>(15, TimeUnit.MINUTES)
                .setConstraints(
                    Constraints.Builder()
                        .setRequiredNetworkType(NetworkType.CONNECTED)
                        .build()
                )
                .build()
            WorkManager.getInstance(ctx).enqueueUniquePeriodicWork(
                PERIODIC,
                // KEEP, not UPDATE: re-enqueueing on every onUpdate would reset
                // the period each time and the refresh could starve.
                ExistingPeriodicWorkPolicy.KEEP,
                req,
            )
        }

        fun refreshNow(ctx: Context) {
            val req = OneTimeWorkRequestBuilder<RefreshWorker>()
                .setConstraints(
                    Constraints.Builder()
                        .setRequiredNetworkType(NetworkType.CONNECTED)
                        .build()
                )
                .build()
            WorkManager.getInstance(ctx)
                .enqueueUniqueWork(ONESHOT, ExistingWorkPolicy.REPLACE, req)
        }

        fun cancel(ctx: Context) {
            WorkManager.getInstance(ctx).cancelUniqueWork(PERIODIC)
        }
    }
}
