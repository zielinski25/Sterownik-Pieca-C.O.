package com.sterownikco.pro.core

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class LocalControllerAddressTest {
    @Test
    fun acceptsRfc1918ControllerAddresses() {
        assertEquals("10.0.0.12", LocalControllerAddress.canonicalPrivateIpv4("10.0.0.12"))
        assertEquals("172.16.1.2", LocalControllerAddress.canonicalPrivateIpv4("172.16.1.2"))
        assertEquals("172.31.255.254", LocalControllerAddress.canonicalPrivateIpv4("172.31.255.254"))
        assertEquals("192.168.10.42", LocalControllerAddress.canonicalPrivateIpv4(" 192.168.10.42 "))
    }

    @Test
    fun rejectsPublicLoopbackLinkLocalAndMalformedAddresses() {
        listOf(
            "8.8.8.8",
            "172.15.0.1",
            "172.32.0.1",
            "192.167.1.1",
            "127.0.0.1",
            "169.254.1.2",
            "0.0.0.0",
            "192.168.1",
            "192.168.1.256",
            "192.168.01.1",
            "controller.local",
            "192.168.1.1:8080",
            "192.168.1.1/attacker.example"
        ).forEach { assertNull("must reject $it", LocalControllerAddress.canonicalPrivateIpv4(it)) }
    }

    @Test
    fun buildsOnlySimpleLocalApiUrls() {
        assertEquals(
            "http://192.168.10.42/api/wifi/list",
            LocalControllerAddress.url("192.168.10.42", "/api/wifi/list")
        )
        assertEquals(
            "http://192.168.10.42/api/rtc-sync?rok=2026&miesiac=10",
            LocalControllerAddress.url("192.168.10.42", "/api/rtc-sync?rok=2026&miesiac=10")
        )
        assertNull(LocalControllerAddress.url("192.168.10.42", "//attacker.example/path"))
        assertNull(LocalControllerAddress.url("192.168.10.42", "/api/wifi/list#fragment"))
        assertNull(LocalControllerAddress.url("192.168.10.42", "/api/%2e%2e/cmd"))
        assertNull(LocalControllerAddress.url("192.168.10.42", "/api/../cmd"))
        assertNull(LocalControllerAddress.url("192.168.10.42", "/api//wifi/list"))
        assertNull(LocalControllerAddress.url("8.8.8.8", "/api/wifi/add"))
    }
}
