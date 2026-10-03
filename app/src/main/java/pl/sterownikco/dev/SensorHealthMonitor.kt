package pl.sterownikco.dev

/**
 * STEROWNIK CO — Intelligent sensor health monitor.
 *
 * Key insight from real data analysis (1504 readings, 25h):
 * - Sensors DON'T always change — stagnation is OFTEN NORMAL!
 *   - t_bojler: can be stable for 142 min when furnace isn't heating CWU
 *   - t_pokoj: can be stable for 363 min (thermally insulated room)
 *   - t_ogrz: can be stable for 98 min when furnace is off
 *   - cisnienie: barely changes (p95 = 0.0 hPa/min!)
 *   - t_zewn: changes max 0.5°C/min (sun/cloud), can be stable 59 min at night
 *
 * This monitor distinguishes:
 * 1. DS18B20 ERROR VALUES → always blocked (handled in AdvancedSensorFilter)
 * 2. Sensor permanently offline (wilgotnosc=0.0 always) → flag OFFLINE
 * 3. Extended stagnation beyond physical expectation → flag WARNING
 * 4. Normal stagnation (e.g., boiler not heating) → NO WARNING
 *
 * Thresholds are per-sensor based on thermal physics:
 * - Outside: changes with weather, stagnation > 60min suspicious (sun/cloud)
 * - Heating: thermal mass of water/iron, can stabilize for 2h
 * - Boiler: large thermal mass, can stabilize for 6h when not heating
 * - Panel: near furnace, can be stable 3h
 * - Room: very stable (walls/furniture absorb heat), 6h normal
 * - Pressure: barely changes, 12h of same value is normal
 * - Humidity: if always 0.0 → sensor disconnected
 */
class SensorHealthMonitor(
    private val sensorName: String,
    /** Maximum minutes the sensor can legitimately show same value */
    private val maxStagnationMinutes: Int = 60,
    /** Value tolerance — values within this range are "same" */
    private val tolerance: Float = 0.05f,
    /** If sensor ALWAYS reads this value → permanently offline */
    private val offlineValues: List<Float> = listOf(0.0f)
) {
    private var lastValue: Float = Float.NaN
    private var stagnationStart: Long = 0  // timestamp when stagnation started
    private var totalReadings: Int = 0
    private var uniqueValues: MutableSet<Float> = mutableSetOf()
    
    data class HealthStatus(
        val isOnline: Boolean,
        val isStagnant: Boolean,
        val stagnationMinutes: Int,
        val statusText: String
    )
    
    /**
     * Add a new reading and return health status.
     * @param value the filtered sensor value
     * @param timestamp in ms
     */
    fun addReading(value: Float, timestamp: Long): HealthStatus {
        totalReadings++
        
        // Track unique values (rounded to tolerance) for offline detection
        val rounded = (value / tolerance).toLong() * tolerance
        uniqueValues.add(rounded)
        
        // Check if sensor is permanently offline (always reads same value)
        if (totalReadings > 50 && uniqueValues.size == 1) {
            val onlyValue = uniqueValues.first()
            if (offlineValues.contains(onlyValue)) {
                return HealthStatus(
                    isOnline = false,
                    isStagnant = false,
                    stagnationMinutes = 0,
                    statusText = "CZUJNIK OFFLINE (brak danych)"
                )
            }
        }
        
        // Check stagnation (same value for extended period)
        val isSameAsLast = lastValue.isFinite() && Math.abs(value - lastValue) <= tolerance
        val stagnationMinutes = if (isSameAsLast && stagnationStart > 0) {
            ((timestamp - stagnationStart) / 60000).toInt()
        } else {
            0
        }
        
        if (!isSameAsLast) {
            stagnationStart = timestamp
        } else if (stagnationStart == 0L) {
            stagnationStart = timestamp
        }
        
        lastValue = value
        
        // Check if stagnation exceeds physical threshold
        val isStagnant = stagnationMinutes > maxStagnationMinutes
        
        val statusText = when {
            isStagnant -> "CZUJNIK ZAMARZNIĘTY? ($stagnationMinutes min bez zmiany, max oczekiwane: ${maxStagnationMinutes} min)"
            else -> "OK"
        }
        
        return HealthStatus(
            isOnline = true,
            isStagnant = isStagnant,
            stagnationMinutes = stagnationMinutes,
            statusText = statusText
        )
    }
    
    fun getTotalReadings(): Int = totalReadings
    fun getUniqueValueCount(): Int = uniqueValues.size
    fun getSensorName(): String = sensorName
    
    fun reset() {
        lastValue = Float.NaN
        stagnationStart = 0
        totalReadings = 0
        uniqueValues.clear()
    }
}
