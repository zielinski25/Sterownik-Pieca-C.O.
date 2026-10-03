package pl.sterownikco.dev

/**
 * STEROWNIK CO — Stagnation detector for sensors.
 * 
 * Problem: DS18B20 sensors can freeze on a single value for hours due to:
 * - 1-Wire bus errors
 * - Power issues
 * - Library bugs
 * - Bad contact
 * 
 * Data shows: t_pokoj=16.0 for 363 readings (6 hours!), t_ogrz=14.7 for 142 readings
 * 
 * This detector tracks when a sensor returns the same value for too long
 * and flags it as "STUCK" so the UI can warn the user.
 */
class StagnationDetector(
    private val sensorName: String = "unknown",
    private val tolerance: Float = 0.1f,  // values within this are considered "same"
    private val maxStableReadings: Int = 10  // flag after N identical readings
) {
    private var lastValue: Float = Float.NaN
    private var stableCount: Int = 0
    private var isStuck: Boolean = false
    
    /**
     * Feed new reading and check if sensor is stuck.
     * @return true if sensor appears stuck (same value too long)
     */
    fun update(rawValue: Float): Boolean {
        if (!rawValue.isFinite()) {
            reset()
            return false
        }
        
        if (lastValue.isNaN() || Math.abs(rawValue - lastValue) > tolerance) {
            // Value changed - reset counter
            lastValue = rawValue
            stableCount = 0
            isStuck = false
        } else {
            // Same value - increment counter
            stableCount++
            if (stableCount >= maxStableReadings && !isStuck) {
                isStuck = true
                android.util.Log.w("StagnationDetector", "$sensorName: STUCK at $rawValue for $stableCount readings")
            }
        }
        
        return isStuck
    }
    
    fun isStuck(): Boolean = isStuck
    fun getStableCount(): Int = stableCount
    fun getLastValue(): Float = lastValue
    
    fun reset() {
        lastValue = Float.NaN
        stableCount = 0
        isStuck = false
    }
}
