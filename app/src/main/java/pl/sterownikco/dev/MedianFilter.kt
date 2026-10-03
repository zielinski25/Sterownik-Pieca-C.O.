package pl.sterownikco.dev

/**
 * STEROWNIK CO — Median filter for sensor data.
 * Removes spikes/anomalies from temperature readings.
 * 
 * Algorithm:
 * 1. Keep buffer of last N values
 * 2. Calculate median
 * 3. Reject values > threshold from median
 * 4. Return smoothed value
 * 
 * Used for: t_zewn, t_ogrz, t_bojler, t_panel, t_pokoj, cisnienie, wilgotnosc
 */
class MedianFilter(private val bufferSize: Int = 10, private val maxDeviation: Float = 5.0f) {
    
    private val buffer = mutableListOf<Float>()
    private var lastValid: Float = Float.NaN
    
    /**
     * Add new value and return filtered result.
     * @param rawValue new sensor reading
     * @return filtered value (median if accepted, last valid if rejected)
     */
    fun filter(rawValue: Float): Float {
        if (!rawValue.isFinite()) return lastValid
        
        buffer.add(rawValue)
        if (buffer.size > bufferSize) {
            buffer.removeAt(0)
        }
        
        // Calculate median
        val sorted = buffer.sorted()
        val median = if (sorted.isEmpty()) {
            rawValue
        } else if (sorted.size % 2 == 0) {
            (sorted[sorted.size / 2 - 1] + sorted[sorted.size / 2]) / 2f
        } else {
            sorted[sorted.size / 2]
        }
        
        // Check if value is within acceptable range
        val deviation = Math.abs(rawValue - median)
        val accepted = deviation <= maxDeviation || buffer.size < 3
        
        return if (accepted) {
            lastValid = rawValue
            rawValue
        } else {
            // Reject spike, return last valid value
            lastValid
        }
    }
    
    /**
     * Get current median without adding new value.
     */
    fun getMedian(): Float {
        if (buffer.isEmpty()) return Float.NaN
        val sorted = buffer.sorted()
        return if (sorted.size % 2 == 0) {
            (sorted[sorted.size / 2 - 1] + sorted[sorted.size / 2]) / 2f
        } else {
            sorted[sorted.size / 2]
        }
    }
    
    /**
     * Reset filter state.
     */
    fun reset() {
        buffer.clear()
        lastValid = Float.NaN
    }
    
    /**
     * Get buffer size (how many values stored).
     */
    fun size(): Int = buffer.size
}
