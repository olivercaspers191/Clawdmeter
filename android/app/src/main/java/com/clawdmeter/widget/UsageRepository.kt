package com.clawdmeter.widget

import android.content.Context
import java.net.HttpURLConnection
import java.net.URL

/**
 * Fetches /usage from the homeserver and caches the last good payload.
 *
 * HttpURLConnection rather than OkHttp: one GET of a few hundred bytes on a LAN
 * or tailnet doesn't justify the dependency.
 */
object UsageRepository {

    private const val PREFS = "clawdmeter"
    private const val KEY_URL = "base_url"
    private const val KEY_LAST = "last_payload"
    private const val KEY_ERROR = "last_error"

    private const val TIMEOUT_MS = 8000

    private fun prefs(c: Context) = c.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    /** Base URL of the usage service, e.g. "http://homeserver:8090". */
    fun baseUrl(c: Context): String = prefs(c).getString(KEY_URL, "") ?: ""

    fun setBaseUrl(c: Context, url: String) {
        // Trailing slashes would produce "http://host:8090//usage".
        prefs(c).edit().putString(KEY_URL, url.trim().trimEnd('/')).apply()
    }

    fun lastError(c: Context): String? = prefs(c).getString(KEY_ERROR, null)

    /** Last successfully fetched payload, with its original fetch time. */
    fun cached(c: Context): UsageData? =
        prefs(c).getString(KEY_LAST, null)?.let { UsageData.parse(it) }

    /**
     * Fetch and cache. Returns the fresh payload, or the cached one when the
     * network fails — the caller renders the cache with its real age rather
     * than blanking the widget on a single dropped request.
     *
     * Blocking; call from a worker thread.
     */
    fun refresh(c: Context): UsageData? {
        val base = baseUrl(c)
        if (base.isEmpty()) {
            prefs(c).edit().putString(KEY_ERROR, "Not configured").apply()
            return cached(c)
        }

        var conn: HttpURLConnection? = null
        return try {
            conn = (URL("$base/usage").openConnection() as HttpURLConnection).apply {
                connectTimeout = TIMEOUT_MS
                readTimeout = TIMEOUT_MS
                requestMethod = "GET"
                setRequestProperty("Accept", "application/json")
                useCaches = false
            }
            val code = conn.responseCode
            if (code != 200) throw Exception("HTTP $code")

            val body = conn.inputStream.bufferedReader().use { it.readText() }
            val data = UsageData.parse(body) ?: throw Exception("bad payload")

            prefs(c).edit()
                .putString(KEY_LAST, data.toJson())
                .remove(KEY_ERROR)
                .apply()
            data
        } catch (e: Exception) {
            prefs(c).edit()
                .putString(KEY_ERROR, e.message ?: e.javaClass.simpleName)
                .apply()
            cached(c)
        } finally {
            conn?.disconnect()
        }
    }
}
