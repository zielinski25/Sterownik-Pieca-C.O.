package com.sterownikco.pro.core

/** Minimal key-value surface used by AlarmCenter; keeps its state logic testable without Android APIs. */
interface AlarmPrefsStore {
    fun get(key: String): String?
    fun set(key: String, value: String)
    fun remove(key: String)
    fun getLong(key: String, def: Long): Long
    fun setLong(key: String, v: Long)
}
