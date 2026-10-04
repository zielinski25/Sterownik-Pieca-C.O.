package pl.sterownikco.dev

import kotlin.math.abs

/**
 * STEROWNIK CO — Advanced sensor filter: sentinel/range check + spike-vs-step discrimination.
 *
 * Problem: DS18B20 sensors on 1-Wire bus produce spikes (85°C default error,
 * random 60-100°C readings) due to EMI, long wires, or bus conflicts.
 *
 * Solution: Four-layer filtering
 * 1. Sentinel values  - DS18B20 error codes (85.0 / -127.0 / -16.0 for t_zewn)
 * 2. Range check      - reject physically impossible values
 * 3. Trend check      - flag a sample that departs from the linear trend of the last two
 *                       accepted samples by more than max(confirmTolerance, rate·Δt)
 * 4. Median deviation - flag outliers against the recent buffer (fallback when layer 3
 *                       has no timing/trend information)
 *
 * Layers 3-4 only mark a sample as SUSPICIOUS. A suspicious sample is rejected once and
 * remembered as a *candidate*. If the very next sample lands at the same new level
 * (within [confirmTolerance]), it is a real STEP — not a glitch — and is accepted at once,
 * with the buffer re-seeded at the new level. This mirrors the firmware's FiltrWiarygodnosci
 * ("odrzuca skok, czeka na potwierdzenie") and fixes the earlier behaviour where a genuine
 * step (pump start, firing up, a sensor simulation from the panel) was held back for
 * `Δ/maxChangePerMinute` minutes.
 *
 * A glitch is a lone sample: 38, 38, 71, 38 → 71 rejected, 38 accepted, candidate dropped.
 * A step persists:           38, 45, 45.5   → 45 rejected once, 45.5 confirms → accepted.
 * A ramp is a trend:         38, 38.8, 39.5, 40.3 (3°C/min) → every sample follows the
 *                            extrapolated trend → accepted without hiccups.
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
    private val absoluteMax: Float = 200f,
    /** How close the confirming sample must be to the candidate to accept a step. */
    private val confirmTolerance: Float = maxOf(maxDeviationFromMedian, 1.0f),
    /** A candidate older than this is forgotten (a late lone sample must not confirm it). */
    private val candidateTtlMs: Long = 10 * 60 * 1000L
) {
    private val buffer = mutableListOf<Float>()
    private var lastValid: Float = Float.NaN
    private var lastTimestamp: Long = 0
    // The accepted sample before lastValid — together they define the current trend.
    private var prevValid: Float = Float.NaN
    private var prevTimestamp: Long = 0
    private var rejectionCount = 0
    private var stepCount = 0
    private var lastRejectionReason: String = ""

    // Pending suspicious sample waiting for confirmation by the next reading.
    private var candidate: Float = Float.NaN
    private var candidateTimestamp: Long = 0

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
        if (abs(rawValue - 85.0f) < 0.15f) {
            return reject("DS18B20 default 85°C")
        }
        if (abs(rawValue - (-127.0f)) < 0.15f) {
            return reject("DS18B20 no sensor -127°C")
        }
        if (abs(rawValue - (-16.0f)) < 0.15f && sensorName == "t_zewn") {
            // -16.0°C observed in real data as bus error (29 consecutive readings)
            // Only block for t_zewn — could be legitimate for other sensors in winter
            return reject("DS18B20 bus error -16°C")
        }

        // Layer 2: Absolute range check
        if (rawValue < absoluteMin || rawValue > absoluteMax) {
            return reject("out of range [$absoluteMin, $absoluteMax]")
        }

        // Layer 3: Trend check (flags only). Predict the value from the linear trend of the
        // last two accepted samples; a departure larger than max(confirmTolerance, rate·Δt)
        // is suspicious. A steady ramp (firing up, boiler heating) follows its own trend and
        // passes; a lone glitch does not.
        var suspicion: String? = null
        var trendChecked = false
        if (timestamp > 0 && lastTimestamp > 0 && lastValid.isFinite()) {
            val dtMs = timestamp - lastTimestamp
            val dtMinutes = dtMs / 60000f
            if (dtMinutes > 0.05f && dtMinutes < 120f) {
                trendChecked = true
                var predicted = lastValid
                if (prevValid.isFinite() && prevTimestamp in 1 until lastTimestamp && lastTimestamp - prevTimestamp < 10 * 60_000L) {
                    val slopePerMs = (lastValid - prevValid) / (lastTimestamp - prevTimestamp).toFloat()
                    predicted = lastValid + slopePerMs * dtMs
                }
                val tolerance = maxOf(confirmTolerance, maxChangePerMinute * dtMinutes)
                val departure = abs(rawValue - predicted)
                if (departure > tolerance) {
                    val rate = abs(rawValue - lastValid) / dtMinutes  // °C per minute
                    suspicion = "jump ${"%.1f".format(departure)}°C off trend (tol ${"%.1f".format(tolerance)}, ${"%.1f".format(rate)}°C/min)"
                }
            }
        }

        // Layer 4: Median deviation against the recent buffer (flags only). Used only when the
        // trend check could not run (no timestamps / first samples / huge gap): the median is a
        // lagging estimate, so on a fast but genuine ramp it would flag samples the trend check
        // already proved plausible.
        if (!trendChecked && suspicion == null && buffer.size >= 3) {
            val median = calculateMedian()
            val deviation = abs(rawValue - median)
            if (deviation > maxDeviationFromMedian) {
                suspicion = "median outlier (dev=${"%.1f".format(deviation)} from median=${"%.1f".format(median)}, max=$maxDeviationFromMedian)"
            }
        }

        // Forget a stale candidate — a late lone sample must not confirm a jump from long ago.
        if (candidate.isFinite() && timestamp > 0 && candidateTimestamp > 0 && timestamp - candidateTimestamp > candidateTtlMs) {
            candidate = Float.NaN
        }

        if (suspicion == null) {
            // Plausible continuation of the current level.
            candidate = Float.NaN
            return accept(rawValue, timestamp)
        }

        // Suspicious. Spike or a real step? A spike is a lone sample; a step persists.
        if (candidate.isFinite() && abs(rawValue - candidate) <= confirmTolerance) {
            // Second consecutive reading at the new level → genuine change
            // (pump start, firing up, simulation injected from the panel). Re-seed at the new level.
            lastRejectionReason = "step confirmed (${"%.1f".format(lastValid)} → ${"%.1f".format(rawValue)})"
            buffer.clear()
            buffer.add(candidate)
            // Seed the trend from the new level only (candidate → raw), not from the old level.
            lastValid = candidate
            lastTimestamp = candidateTimestamp
            candidate = Float.NaN
            stepCount++
            return accept(rawValue, timestamp)
        }

        candidate = rawValue
        candidateTimestamp = timestamp
        return reject(suspicion)
    }

    /** Accept [value] as the new current level without any checks (e.g. a known simulation). */
    fun acceptUnfiltered(value: Float, timestamp: Long = 0): Float {
        if (!value.isFinite()) return lastValid
        buffer.clear()
        candidate = Float.NaN
        prevValid = Float.NaN
        prevTimestamp = 0
        // Flat trend at the injected level.
        lastValid = value
        lastTimestamp = timestamp
        return accept(value, timestamp)
    }

    private fun accept(value: Float, timestamp: Long): Float {
        buffer.add(value)
        if (buffer.size > bufferSize) buffer.removeAt(0)
        if (timestamp > lastTimestamp) {
            prevValid = lastValid
            prevTimestamp = lastTimestamp
        }
        lastValid = value
        if (timestamp > 0) lastTimestamp = timestamp
        return value
    }

    private fun reject(reason: String): Float {
        lastRejectionReason = reason
        rejectionCount++
        return lastValid
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
    fun getStepCount(): Int = stepCount
    fun getBufferSize(): Int = buffer.size
    fun getName(): String = sensorName
    fun getLastRejectionReason(): String = lastRejectionReason
    fun hasPendingCandidate(): Boolean = candidate.isFinite()

    fun reset() {
        buffer.clear()
        lastValid = Float.NaN
        lastTimestamp = 0
        rejectionCount = 0
        stepCount = 0
        lastRejectionReason = ""
        candidate = Float.NaN
        candidateTimestamp = 0
        prevValid = Float.NaN
        prevTimestamp = 0
    }
}
