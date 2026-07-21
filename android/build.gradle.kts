// Versions are pinned to a known-good combination. If Android Studio offers to
// upgrade AGP/Kotlin on first open, accepting is fine — nothing here depends on
// a specific version's behaviour.
plugins {
    id("com.android.application") version "8.6.1" apply false
    id("org.jetbrains.kotlin.android") version "2.0.20" apply false
}
