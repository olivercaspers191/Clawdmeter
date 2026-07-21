package com.clawdmeter.widget

import org.json.JSONObject

/**
 * The homeserver's short-key usage payload — the same JSON the ESP32 parses in
 * firmware/src/main.cpp::parse_json(). Keys documented in homeserver/README.md.
 *
 * Only the fields this widget renders are modelled; unknown keys are ignored so
 * the service can grow without breaking the app.
 */
data class UsageData(
    val sessionPct: Float,
    val sessionResetMins: Int,
    val weeklyPct: Float,
    val weeklyResetMins: Int,
    /** Per-model weekly bar (today: Fable). Null when the payload omits "f". */
    val modelPct: Float?,
    val modelResetMins: Int,
    val modelName: String,
    /** Wall-clock ms when this payload was fetched, for the "Nm ago" line. */
    val fetchedAt: Long,
) {
    fun toJson(): String = JSONObject().apply {
        put("s", sessionPct.toDouble())
        put("sr", sessionResetMins)
        put("w", weeklyPct.toDouble())
        put("wr", weeklyResetMins)
        modelPct?.let { put("f", it.toDouble()) }
        put("fr", modelResetMins)
        put("fn", modelName)
        put("at", fetchedAt)
    }.toString()

    companion object {
        /**
         * @param fetchedAt when the payload arrived. Read back from the stored
         *   "at" key when reloading from disk, so a cached payload keeps its
         *   original age instead of appearing freshly fetched.
         */
        fun parse(body: String, fetchedAt: Long = System.currentTimeMillis()): UsageData? {
            return try {
                val o = JSONObject(body)
                if (o.has("ok") && !o.optBoolean("ok", false)) return null
                UsageData(
                    sessionPct = o.optDouble("s", 0.0).toFloat(),
                    sessionResetMins = o.optInt("sr", -1),
                    weeklyPct = o.optDouble("w", 0.0).toFloat(),
                    weeklyResetMins = o.optInt("wr", -1),
                    // has() not optDouble-with-default: a genuine 0% Fable bar
                    // must still render, so absence has to be distinct from zero.
                    modelPct = if (o.has("f")) o.optDouble("f", 0.0).toFloat() else null,
                    modelResetMins = o.optInt("fr", -1),
                    modelName = o.optString("fn", "Model"),
                    fetchedAt = o.optLong("at", fetchedAt),
                )
            } catch (e: Exception) {
                null
            }
        }
    }
}
