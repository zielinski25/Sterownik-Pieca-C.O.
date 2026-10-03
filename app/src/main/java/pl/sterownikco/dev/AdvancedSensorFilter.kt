package pl.sterownikco.dev

/**
 * STEROWNIK CO — Advanced sensor filter combining median + MAD + rate-limit.
 *
 * Problem: DS18B20 sensors on 1-Wire bus produce spikes (85°C default error,
 * random 60-100°C readings) due to EMI, long wires, or bus conflicts.
 *
 * Solution: Three-layer filtering
 * 1. Range check - reject physically impossible values
 * 2. Rate limiting (time-scaled) - reject jumps faster than physically possible
 * 3. Median filter - reject outliers from buffer of N values
 *
 * Thresholds tuned from real data analysis (1504 readings, 1-min intervals):
 *   t_zewn:  max legitimate change = 0.5°C/min (sun/cloud)
 *   t_ogrz:  max = 0.3°C/min (slow heating/cooling)
 *   t_bojler: max = 0.3°C/min
 *   t_panel: max = 0.3°C/min (up to 0.5 in direct sun)
 *   t_pokoj: max = 0.3°C/min (thermally stable room)
 *   cisnienie: max = 0.5 hPa/min
 */
class AdvancedSensorFilter(
    private val sensorName: String = "unknown",
    private val bufferSize: Int = 8,
    private val maxDeviationFromMedian: Float = 3.0f,
    /** Maximum legitimate change per minute (°C/min). Scaled to actual Δt. */
    private val maxChangePerMinute: Float = 1.0f,
    private val absoluteMin: Float = -50f,
    private val absoluteMax: Float = 200f
) {
    private val buffer = mutableListOf<Float>()
    private var lastValid: Float = Float.NaN
    private var lastTimestamp: Long = 0
    private var rejectionCount = 0
    private var lastRejectionReason: String = ""

    /**
     * Filter new sensor reading.
     * @param rawValue new measurement
     * @param timestamp optional timestamp in ms for rate limiting
     * @return filtered value
     */
    fun filter(rawValue: Float, timestamp: Long = 0): Float {
        if (!rawValue.isFinite()) {
            return lastValid
        }

        // Layer 1: DS18B20 known error values (before range check)
        // 85.0°C = power-on default, -127.0°C = no sensor, -16.0°C = bus error (observed in real data)
        if (Math.abs(rawValue - 85.0f) < 0.15f) {
            lastRejectionReason = "DS18B20 default 85°C"
            rejectionCount++
            return lastValid
        }
        if (Math.abs(rawValue - (-127.0f)) < 0.15f) {
            lastRejectionReason = "DS18B20 no sensor -127°C"
            rejectionCount++
            return lastValid
        }
        if (Math.abs(rawValue - (-16.0f)) < 0.15f && sensorName == "t_zewn") {
            // -16.0°C observed in real data as bus error (29 consecutive readings)
            // Only block for t_zewn — could be legitimate for other sensors in winter
            lastRejectionReason = "DS18B20 bus error -16°C"
            rejectionCount++
            return lastValid
        }

        // Layer 2: Absolute range check
        if (rawValue < absoluteMin || rawValue > absoluteMax) {
            lastRejectionReason = "out of range [$absoluteMin, $absoluteMax]"
            rejectionCount++
            return lastValid
        }

        // Layer 3: Time-scaled rate limiting
        // Real data shows max change per minute:
        //   t_zewn=0.5, t_ogrz/t_bojler/t_panel/t_pokoj=0.3, cisnienie=0.5
        // We set threshold at 3× observed max to allow legitimate rapid changes
        if (timestamp > 0 && lastTimestamp > 0 && lastValid.isFinite()) {
            val dtMinutes = (timestamp - lastTimestamp) / 60000f
            if (dtMinutes > 0.1f && dtMinutes < 120f) {
                val change = Math.abs(rawValue - lastValid)
                val rate = change / dtMinutes  // °C per minute
                if (rate > maxChangePerMinute) {
                    lastRejectionReason = "rate spike ${rate}°C/min (max $maxChangePerMinute)"
                    rejectionCount++
                    return lastValid
                }
            }
        }

        // Layer 4: Median filter (catch outliers that pass rate limit)
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
            lastRejectionReason = "median outlier (dev=$deviation from median=$median, max=$maxDeviationFromMedian)"
            rejectionCount++
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
    fun getName(): String = sensorName
    fun getLastRejectionReason(): String = lastRejectionReason

    fun reset() {
        buffer.clear()
        lastValid = Float.NaN
        lastTimestamp = 0
        rejectionCount = 0
        lastRejectionReason = ""
    }
}
