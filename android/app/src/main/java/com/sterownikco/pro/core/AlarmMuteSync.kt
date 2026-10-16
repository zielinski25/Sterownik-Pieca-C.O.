package com.sterownikco.pro.core

import android.content.Context
import com.sterownikco.pro.service.AlarmMonitorService

/** Routes user mute actions through the running alarm service so the controller can ACK them. */
internal object AlarmMuteSync {
    /** seconds=null explicitly re-enables the selected active alarm classes; zero means until clear. */
    fun request(context: Context, signature: String, seconds: Int?) {
        val commands = AlarmMuteCommands.forMute(signature, seconds)
        if (commands.isEmpty()) return
        AlarmMonitorService.requestControllerMute(context.applicationContext, signature, seconds)
    }
}
