package pl.sterownikco.dev

/**
 * STEROWNIK CO — Advanced sensor filter combining median + MAD + rate-limit.
 * 
 * Problem: DS18B20 sensors on 1-Wire bus produce spikes (85°C default error,
 * random 60-100°C readings) due to EMI, long wires, or bus conflicts.
 * 
 * Solution: Three-layer filtering
 * 1. Range check - reject physically impossible values
 * 2. Median filter - reject outliers from buffer of N values
 * 3. Rate limiting - reject sudden jumps > maxChangePerUpdate
 * 
 * Matches firmware's FiltrWiarygodnosci + spurious_85C filter.
 */
class AdvancedSensorFilter(
    private val sensorName: String = "unknown",
    private val bufferSize: Int = 8,
    private val maxDeviationFromMedian: Float = 3.0f,
    private val maxChangePerUpdate: Float = 2.0f,
    private val absoluteMin: Float = -50f,
    private val absoluteMax: Float = 200f
) {
    private val buffer = mutableListOf<Float>()
    private var lastValid: Float = Float.NaN
    private var lastTimestamp: Long = 0
    private var rejectionCount = 0
    
    /**
     * Filter new sensor reading.
     * @param rawValue new measurement
     * @param timestamp optional timestamp for rate limiting (0 = no rate limit)
     * @return filtered value
     */
    fun filter(rawValue: Float, timestamp: Long = 0): Float {
        if (!rawValue.isFinite()) {
            return lastValid
        }
        
        // Layer 1: Absolute range check (catch 85°C error, negatives, etc.)
        if (rawValue < absoluteMin || rawValue > absoluteMax) {
            rejectionCount++
            if (rejectionCount % 10 == 0) {
                android.util.Log.w("SensorFilter", "$sensorName: rejected out-of-range $rawValue (count=$rejectionCount)")
            }
            return lastValid
        }
        
        // Special case: DS18B20 default error value (85.0°C exactly)
        if (Math.abs(rawValue - 85.0f) < 0.1f) {
            rejectionCount++
            if (rejectionCount % 10 == 0) {
                android.util.Log.w("SensorFilter", "$sensorName: rejected DS18B20 default 85°C (count=$rejectionCount)")
            }
            return lastValid
        }
        
        // Layer 2: Rate limiting (catch sudden jumps)
        if (timestamp > 0 && lastTimestamp > 0 && lastValid.isFinite()) {
            val dt = (timestamp - lastTimestamp) / 1000f // seconds
            if (dt > 0 && dt < 60f) { // only check if updates are frequent
                val rate = Math.abs(rawValue - lastValid) / dt
                // Typical temp change rate: < 0.5°C/s for heating, < 2°C/s for panel sun
                val maxRate = if (sensorName.contains("panel")) 2.0f else 0.5f
                if (rate > maxRate * 10f) { // allow some burst
                    rejectionCount++
                    if (rejectionCount % 10 == 0) {
                        android.util.Log.w("SensorFilter", "$sensorName: rejected rate spike ${rate}°C/s (count=$rejectionCount)")
                    }
                    return lastValid
                }
            }
        }
        
        // Layer 3: Median filter
        buffer.add(rawValue)
        if (buffer.size > bufferSize) {
            buffer.removeAt(0)
        }
        
        val median = calculateMedian()
        val deviation = Math.abs(rawValue - median)
        
        val accepted = if (buffer.size < 3) {
            true // not enough data for median, accept
        } else {
            deviation <= maxDeviationFromMedian
        }
        
        return if (accepted) {
            lastValid = rawValue
            lastTimestamp = timestamp
            rawValue
        } else {
            rejectionCount++
            if (rejectionCount % 10 == 0) {
                android.util.Log.w("SensorFilter", "$sensorName: rejected median outlier $rawValue (median=$median, dev=$deviation, count=$rejectionCount)")
            }
            lastValid
        }
    }
    
    private fun calculateMedian(): Float {
        if (buffer.isEmpty()) return Float.NaN
        val sorted = buffer.sorted()
        return if (sorted.size % 2 == 0) {
            (sorted[sorted.size / 2 - 1] + sorted[sorted.size / 2]) / 2f
        } else {
            sorted[sorted.size / 2]
        }
    }
    
    fun getMedian(): Float = calculateMedian()
    fun getLastValid(): Float = lastValid
    fun getRejectionCount(): Int = rejectionCount
    fun getBufferSize(): Int = buffer.size
    
    fun reset() {
        buffer.clear()
        lastValid = Float.NaN
        lastTimestamp = 0
        rejectionCount = 0
    }
}
