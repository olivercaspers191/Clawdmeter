package com.clawdmeter.widget

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.RectF
import android.graphics.Typeface
import androidx.core.content.res.ResourcesCompat
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/**
 * Draws the widget to a Bitmap.
 *
 * Canvas rather than a RemoteViews layout tree, for two reasons:
 *  1. Arbitrary resizing. The user can stretch this to 2x2, 3x3, 4x1, whatever;
 *     rather than switching between a handful of fixed layouts, everything is
 *     computed from the measured size, so every intermediate size looks
 *     deliberate rather than like the nearest tier stretched.
 *  2. Exact parity with the panel and the web view — real Styrene, the same
 *     pill-radius bars, the same colours from firmware/src/theme.h. RemoteViews
 *     can't restyle a ProgressBar's fill colour per-update below API 31.
 */
object WidgetRenderer {

    // ---- design tokens, mirrored from firmware/src/theme.h ----
    private const val BG = 0xFF000000.toInt()
    private const val PANEL = 0xFF1F1F1E.toInt()
    private const val TEXT = 0xFFFAF9F5.toInt()
    private const val DIM = 0xFFB0AEA5.toInt()
    private const val GREEN = 0xFF788C5D.toInt()
    private const val AMBER = 0xFFD97757.toInt()
    private const val RED = 0xFFC0392B.toInt()
    private const val BAR_BG = 0xFF2A2A28.toInt()

    // ---- layout thresholds, in dp ----
    private const val PAD = 8f
    private const val GAP = 6f
    private const val PANEL_MIN_H = 40f      // below this a panel can't hold % + bar
    private const val PANEL_RESET_MIN_H = 68f // ...and below this, no room for "Resets in"
    private const val HEADER_H = 26f
    private const val HEADER_MIN_TOTAL_H = 170f
    private const val ROW_MODE_MAX_H = 92f   // shorter than this -> lay panels side by side

    /** A widget bitmap far larger than this risks the host's bitmap-memory cap. */
    private const val MAX_PX = 1400

    /** firmware/src/ui.cpp pct_color() */
    private fun pctColor(p: Float) = when {
        p >= 80f -> RED
        p >= 50f -> AMBER
        else -> GREEN
    }

    /** firmware/src/ui.cpp format_reset_time() */
    fun formatReset(mins: Int): String = when {
        mins < 0 -> "---"
        mins < 60 -> "Resets in ${mins}m"
        mins < 1440 -> "Resets in ${mins / 60}h ${mins % 60}m"
        else -> "Resets in ${mins / 1440}d ${(mins % 1440) / 60}h"
    }

    private fun ageText(fetchedAt: Long): String {
        val mins = ((System.currentTimeMillis() - fetchedAt) / 60000L).toInt()
        return when {
            mins < 1 -> "just now"
            mins < 60 -> "${mins}m ago"
            mins < 1440 -> "${mins / 60}h ago"
            else -> "${mins / 1440}d ago"
        }
    }

    private data class Panel(val pct: Float, val label: String, val resetMins: Int)

    fun render(ctx: Context, widthDp: Int, heightDp: Int, data: UsageData?, error: String?): Bitmap {
        val density = ctx.resources.displayMetrics.density
        fun dp(v: Float) = v * density

        // Clamp before allocating: a host that asks for something absurd should
        // get a smaller bitmap scaled up, not an OutOfMemoryError.
        val w = min((widthDp * density).roundToInt().coerceAtLeast(1), MAX_PX)
        val h = min((heightDp * density).roundToInt().coerceAtLeast(1), MAX_PX)

        val bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        val c = Canvas(bmp)

        val font = runCatching { ResourcesCompat.getFont(ctx, R.font.styrene_b) }.getOrNull()
            ?: Typeface.DEFAULT
        val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { typeface = font }

        // Rounded black card. Launchers already clip widgets to a rounded shape
        // on Android 12+, but drawing it keeps older versions looking right.
        paint.color = BG
        val radius = dp(16f)
        c.drawRoundRect(RectF(0f, 0f, w.toFloat(), h.toFloat()), radius, radius, paint)

        if (data == null) {
            drawMessage(c, w, h, paint, dp(14f), error ?: "Tap to set up")
            return bmp
        }

        val panels = buildList {
            add(Panel(data.sessionPct, "Current", data.sessionResetMins))
            add(Panel(data.weeklyPct, "Weekly", data.weeklyResetMins))
            data.modelPct?.let { add(Panel(it, "${data.modelName} 1W", data.modelResetMins)) }
        }

        val pad = dp(PAD)
        val gap = dp(GAP)

        // --- short and wide: lay the panels out side by side instead of stacked ---
        if (heightDp < ROW_MODE_MAX_H) {
            val n = min(panels.size, max(1, floor((widthDp - 2 * PAD + GAP) / 78f).toInt()))
            val colW = (w - 2 * pad - (n - 1) * gap) / n
            for (i in 0 until n) {
                val left = pad + i * (colW + gap)
                drawPanel(c, paint, RectF(left, pad, left + colW, h - pad), panels[i], dp(0f), density)
            }
            return bmp
        }

        // --- stacked ---
        var top = pad
        val showHeader = heightDp >= HEADER_MIN_TOTAL_H
        if (showHeader) {
            val hh = dp(HEADER_H)
            drawHeader(ctx, c, paint, RectF(pad, top, w - pad, top + hh), data, density)
            top += hh + gap
        }

        val avail = (h - pad) - top
        // How many panels actually fit at a legible height? Fewer, larger panels
        // beat cramming all three into a 2x2 — the top bar is the one that matters.
        val maxFit = floor((avail + gap) / (dp(PANEL_MIN_H) + gap)).toInt()
        val n = panels.size.coerceAtMost(max(1, maxFit))
        val panelH = (avail - (n - 1) * gap) / n

        for (i in 0 until n) {
            val t = top + i * (panelH + gap)
            drawPanel(c, paint, RectF(pad, t, w - pad, t + panelH), panels[i], dp(PANEL_RESET_MIN_H), density)
        }
        return bmp
    }

    private fun drawMessage(c: Canvas, w: Int, h: Int, paint: Paint, size: Float, msg: String) {
        paint.color = DIM
        paint.textSize = size
        paint.textAlign = Paint.Align.CENTER
        val fm = paint.fontMetrics
        c.drawText(msg, w / 2f, h / 2f - (fm.ascent + fm.descent) / 2f, paint)
        paint.textAlign = Paint.Align.LEFT
    }

    private fun drawHeader(
        ctx: Context, c: Canvas, paint: Paint, r: RectF, data: UsageData, density: Float,
    ) {
        // Pixel-art crab: decode unscaled and blit with filtering off, or the
        // launcher's density scaling turns it to mush.
        val opts = BitmapFactory.Options().apply { inScaled = false }
        val logo = runCatching {
            BitmapFactory.decodeResource(ctx.resources, R.drawable.logo, opts)
        }.getOrNull()
        if (logo != null) {
            val s = r.height()
            val dst = RectF(r.left, r.top, r.left + s, r.top + s)
            val p = Paint().apply { isFilterBitmap = false; isDither = false }
            c.drawBitmap(logo, Rect(0, 0, logo.width, logo.height), dst, p)
        }

        paint.color = DIM
        paint.textSize = min(r.height() * 0.52f, 13f * density)
        paint.textAlign = Paint.Align.RIGHT
        val fm = paint.fontMetrics
        c.drawText(ageText(data.fetchedAt), r.right, r.centerY() - (fm.ascent + fm.descent) / 2f, paint)
        paint.textAlign = Paint.Align.LEFT
    }

    /**
     * One usage panel. Every dimension is a fraction of the panel's own height,
     * so the same code draws a chunky 4x4 panel and a squat 4x1 one.
     *
     * @param resetMinH panel height below which the "Resets in" line is dropped;
     *                  pass 0 to always drop it (row mode has no vertical room).
     */
    private fun drawPanel(
        c: Canvas, paint: Paint, r: RectF, panel: Panel, resetMinH: Float, density: Float,
    ) {
        fun dp(v: Float) = v * density
        val H = r.height()

        paint.color = PANEL
        val rad = dp(10f)
        c.drawRoundRect(r, rad, rad, paint)

        val padH = dp(10f)
        val padV = H * 0.12f
        val left = r.left + padH
        val right = r.right - padH
        val avail = H - 2 * padV
        val showReset = resetMinH > 0f && H >= resetMinH

        // Vertical budget as fractions of the usable height. The two cases sum
        // to 1.0 so nothing overflows the panel at any size.
        val pctH: Float; val gap1: Float; val barH: Float; val gap2: Float; val resetH: Float
        if (showReset) {
            pctH = avail * 0.44f; gap1 = avail * 0.06f
            barH = min(avail * 0.14f, dp(14f)); gap2 = avail * 0.06f
            resetH = avail * 0.30f
        } else {
            pctH = avail * 0.58f; gap1 = avail * 0.12f
            barH = min(avail * 0.30f, dp(14f)); gap2 = 0f; resetH = 0f
        }

        var y = r.top + padV

        // ---- big percentage (left) + pill label (right) ----
        paint.color = TEXT
        paint.textSize = min(pctH * 0.95f, dp(44f))
        val fm = paint.fontMetrics
        val pctBaseline = y + pctH / 2f - (fm.ascent + fm.descent) / 2f
        val pctText = "${panel.pct.roundToInt()}%"
        c.drawText(pctText, left, pctBaseline, paint)

        // Pill only when it won't collide with the number.
        val pctW = paint.measureText(pctText)
        val pillTextSize = min(pctH * 0.40f, dp(15f))
        paint.textSize = pillTextSize
        val labelW = paint.measureText(panel.label)
        val pillPadH = pillTextSize * 0.85f
        val pillPadV = pillTextSize * 0.42f
        val pillW = labelW + 2 * pillPadH
        val pillH = pillTextSize + 2 * pillPadV
        if (left + pctW + dp(6f) + pillW <= right) {
            val pillRect = RectF(right - pillW, y + (pctH - pillH) / 2f, right, y + (pctH + pillH) / 2f)
            paint.color = BAR_BG
            c.drawRoundRect(pillRect, pillH / 2f, pillH / 2f, paint)
            paint.color = TEXT
            val pfm = paint.fontMetrics
            c.drawText(panel.label, pillRect.left + pillPadH,
                pillRect.centerY() - (pfm.ascent + pfm.descent) / 2f, paint)
        }
        y += pctH + gap1

        // ---- bar: pill-rounded track + fill, coloured by level ----
        val barRect = RectF(left, y, right, y + barH)
        paint.color = BAR_BG
        c.drawRoundRect(barRect, barH / 2f, barH / 2f, paint)

        val frac = (panel.pct / 100f).coerceIn(0f, 1f)
        // Floor the fill at one bar-height so a low percentage still renders as
        // a visible pill rather than a sliver, matching the device.
        val fillW = min(max(barH, barRect.width() * frac), barRect.width())
        paint.color = pctColor(panel.pct)
        c.drawRoundRect(RectF(left, y, left + fillW, y + barH), barH / 2f, barH / 2f, paint)
        y += barH + gap2

        // ---- reset line ----
        if (showReset) {
            paint.color = DIM
            paint.textSize = min(resetH * 0.72f, dp(14f))
            val rfm = paint.fontMetrics
            c.drawText(formatReset(panel.resetMins), left,
                y + resetH / 2f - (rfm.ascent + rfm.descent) / 2f, paint)
        }
    }
}
