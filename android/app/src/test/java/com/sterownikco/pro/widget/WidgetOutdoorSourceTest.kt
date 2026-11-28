package com.sterownikco.pro.widget

import org.junit.Assert.assertEquals
import org.junit.Test

class WidgetOutdoorSourceTest {
    @Test fun outdoorSlotFollowsTheSharedChoice() {
        assertEquals("t_zewn", WidgetOutdoorSource.key("t_zewn", 1))
        assertEquals("t_zewn_2", WidgetOutdoorSource.key("t_zewn", 2))
        assertEquals("t_zewn", WidgetOutdoorSource.key("t_zewn", -1))
        assertEquals("t_bojler", WidgetOutdoorSource.key("t_bojler", 2))
    }
}
