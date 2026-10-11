package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Test

class TileReorderTest {
    private val definitions = listOf(
        DashboardTileDef("outdoor", "Outdoor", 1, "Measurement"),
        DashboardTileDef("boiler", "Boiler", 1, "Measurement"),
        DashboardTileDef("panel", "Panel", 1, "Measurement"),
        DashboardTileDef("pump", "Pump", 2, "Control"),
        DashboardTileDef("servo", "Servo", 2, "Control")
    )

    @Test
    fun movesTileToDropTargetSlotWithoutReorderingOtherSection() {
        val current = listOf("outdoor", "boiler", "panel", "pump", "servo")

        val reordered = reorderTileWithinSection(current, definitions, "outdoor", "panel")

        assertEquals(listOf("boiler", "panel", "outdoor", "pump", "servo"), reordered)
    }

    @Test
    fun movesTileUpWithinSameSection() {
        val current = listOf("outdoor", "boiler", "panel", "pump", "servo")

        val reordered = reorderTileWithinSection(current, definitions, "panel", "outdoor")

        assertEquals(listOf("panel", "outdoor", "boiler", "pump", "servo"), reordered)
    }

    @Test
    fun ignoresUnknownTilesAndCrossSectionDrops() {
        val current = listOf("outdoor", "boiler", "pump", "servo")

        assertEquals(current, reorderTileWithinSection(current, definitions, "outdoor", "pump"))
        assertEquals(current, reorderTileWithinSection(current, definitions, "missing", "outdoor"))
    }
}
