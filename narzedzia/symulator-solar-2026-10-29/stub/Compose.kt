// STUB tylko na potrzeby symulacji — zastępuje androidx.compose.runtime.
// Delegaty zachowują się jak w Compose: `var x by mutableStateOf(v)`.
package androidx.compose.runtime

import kotlin.reflect.KProperty

class MutableStateImpl<T>(initial: T) {
    var value: T = initial
}
class MutableDoubleState(initial: Double) { var doubleValue: Double = initial }
class MutableIntState(initial: Int) { var intValue: Int = initial }

fun <T> mutableStateOf(initial: T): MutableStateImpl<T> = MutableStateImpl(initial)
fun mutableDoubleStateOf(initial: Double): MutableDoubleState = MutableDoubleState(initial)
fun mutableIntStateOf(initial: Int): MutableIntState = MutableIntState(initial)

operator fun <T> MutableStateImpl<T>.getValue(thisRef: Any?, property: KProperty<*>): T = value
operator fun <T> MutableStateImpl<T>.setValue(thisRef: Any?, property: KProperty<*>, v: T) { value = v }
operator fun MutableDoubleState.getValue(thisRef: Any?, property: KProperty<*>): Double = doubleValue
operator fun MutableDoubleState.setValue(thisRef: Any?, property: KProperty<*>, v: Double) { doubleValue = v }
operator fun MutableIntState.getValue(thisRef: Any?, property: KProperty<*>): Int = intValue
operator fun MutableIntState.setValue(thisRef: Any?, property: KProperty<*>, v: Int) { intValue = v }
