package com.civichud.maintenance.identity

import org.junit.Assert.*
import org.junit.Test

class AccountConfigurationTest {
    private val config=AccountConfiguration("https://login.example.org/.well-known/openid-configuration","client","api/scope","https://api.example.org")
    @Test fun absentRegistrationDisablesLogin(){assertFalse(config.copy(clientId="").ready);assertFalse(config.copy(discoveryUrl="").ready);assertFalse(config.copy(scope="").ready)}
    @Test fun validHttpsPublicConfigEnablesPreparation(){assertTrue(config.ready)}
    @Test fun insecureOrCredentialBearingConfigRejected(){assertFalse(config.copy(apiUrl="http://api.example.org").ready);assertFalse(config.copy(discoveryUrl="https://secret@login.example.org/discovery").ready)}
    @Test fun apiQueryFragmentAndUnexpectedPortRejected(){assertFalse(config.copy(apiUrl="https://api.example.org?key=x").ready);assertFalse(config.copy(apiUrl="https://api.example.org/#x").ready);assertFalse(config.copy(apiUrl="https://api.example.org:8080").ready)}
    @Test fun linkingOnlyTrustedHosts(){assertTrue(config.permitsLink("https://login.example.org/authorize?ticket=opaque"));assertTrue(config.permitsLink("https://api.example.org/link/one-time"))}
    @Test fun linkingRejectsLookalikesAndNonHttps(){assertFalse(config.permitsLink("https://login.example.org.evil.org/link"));assertFalse(config.permitsLink("http://api.example.org/link"));assertFalse(config.permitsLink("https://api.example.org@evil.org/link"));assertFalse(config.permitsLink("javascript:alert(1)"))}
}
