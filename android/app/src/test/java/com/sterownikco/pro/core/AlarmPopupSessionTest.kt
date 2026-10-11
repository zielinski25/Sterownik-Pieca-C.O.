package com.sterownikco.pro.core

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AlarmPopupSessionTest {
    @Test
    fun activeAlarmCanRestoreWhenPreviewHasNotBeenDismissed() {
        assertTrue(AlarmPopupSession().shouldRestore("D"))
    }

    @Test
    fun dismissedPreviewDoesNotReopenOnEveryPoll() {
        val session = AlarmPopupSession()
        session.dismiss("D")

        assertFalse(session.shouldRestore("D"))
    }

    @Test
    fun newAlarmEventCanShowSameSignatureAgain() {
        val session = AlarmPopupSession()
        session.dismiss("D")
        session.onAlarmEvent()

        assertTrue(session.shouldRestore("D"))
    }

    @Test
    fun differentAlarmSignatureCanOpenAfterDismissal() {
        val session = AlarmPopupSession()
        session.dismiss("D")

        assertTrue(session.shouldRestore("D+O"))
    }

    @Test
    fun clearingAlarmAllowsSameSignatureToShowAgainLater() {
        val session = AlarmPopupSession()
        session.dismiss("D")
        session.clear()

        assertTrue(session.shouldRestore("D"))
    }
}
