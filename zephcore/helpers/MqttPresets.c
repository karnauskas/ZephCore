/* SPDX-License-Identifier: MIT
 * Preset data: agessaman/MeshCore 860011d2dc4d01ecc3719f795821e4a755970a53.
 */
#include "MqttPresets.h"
#include <string.h>
static const char MQTT_USERPASS_USERNAME_PUBKEY[] = "{pubkey}";
// Google Trust Services - GTS Root R4 (used by LetsMesh Analyzer)
static const char GTS_ROOT_R4[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDejCCAmKgAwIBAgIQf+UwvzMTQ77dghYQST2KGzANBgkqhkiG9w0BAQsFADBX\n"
    "MQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTEQMA4GA1UE\n"
    "CxMHUm9vdCBDQTEbMBkGA1UEAxMSR2xvYmFsU2lnbiBSb290IENBMB4XDTIzMTEx\n"
    "NTAzNDMyMVoXDTI4MDEyODAwMDA0MlowRzELMAkGA1UEBhMCVVMxIjAgBgNVBAoT\n"
    "GUdvb2dsZSBUcnVzdCBTZXJ2aWNlcyBMTEMxFDASBgNVBAMTC0dUUyBSb290IFI0\n"
    "MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE83Rzp2iLYK5DuDXFgTB7S0md+8Fhzube\n"
    "Rr1r1WEYNa5A3XP3iZEwWus87oV8okB2O6nGuEfYKueSkWpz6bFyOZ8pn6KY019e\n"
    "WIZlD6GEZQbR3IvJx3PIjGov5cSr0R2Ko4H/MIH8MA4GA1UdDwEB/wQEAwIBhjAd\n"
    "BgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwDwYDVR0TAQH/BAUwAwEB/zAd\n"
    "BgNVHQ4EFgQUgEzW63T/STaj1dj8tT7FavCUHYwwHwYDVR0jBBgwFoAUYHtmGkUN\n"
    "l8qJUC99BM00qP/8/UswNgYIKwYBBQUHAQEEKjAoMCYGCCsGAQUFBzAChhpodHRw\n"
    "Oi8vaS5wa2kuZ29vZy9nc3IxLmNydDAtBgNVHR8EJjAkMCKgIKAehhxodHRwOi8v\n"
    "Yy5wa2kuZ29vZy9yL2dzcjEuY3JsMBMGA1UdIAQMMAowCAYGZ4EMAQIBMA0GCSqG\n"
    "SIb3DQEBCwUAA4IBAQAYQrsPBtYDh5bjP2OBDwmkoWhIDDkic574y04tfzHpn+cJ\n"
    "odI2D4SseesQ6bDrarZ7C30ddLibZatoKiws3UL9xnELz4ct92vID24FfVbiI1hY\n"
    "+SW6FoVHkNeWIP0GCbaM4C6uVdF5dTUsMVs/ZbzNnIdCp5Gxmx5ejvEau8otR/Cs\n"
    "kGN+hr/W5GvT1tMBjgWKZ1i4//emhA1JG1BbPzoLJQvyEotc03lXjTaCzv8mEbep\n"
    "8RqZ7a2CPsgRbuvTPBwcOMBBmuFeU88+FSBX6+7iP0il8b4Z0QFqIwwMHfs/L6K1\n"
    "vepuoxtGzi4CZ68zJpiq1UvSqTbFJjtbD4seiMHl\n"
    "-----END CERTIFICATE-----\n";

// ISRG Root X1 (used by MeshMapper - Let's Encrypt root CA)
static const char ISRG_ROOT_X1[] =
    "-----BEGIN CERTIFICATE-----\n"
    "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\n"
    "-----END CERTIFICATE-----\n";

// Number of built-in presets


// Built-in preset definitions (stored in flash)
const struct MQTTPresetDef MQTT_PRESETS[MQTT_PRESET_COUNT] = {
    //   name           url                                      audience                           rootCA         auth                topic                 tokenLife retain keepAlive user         pass
    { "analyzer-us",   "wss://mqtt-us-v1.letsmesh.net:443/mqtt",  "mqtt-us-v1.letsmesh.net",         GTS_ROOT_R4,   MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "analyzer-eu",   "wss://mqtt-eu-v1.letsmesh.net:443/mqtt",  "mqtt-eu-v1.letsmesh.net",         GTS_ROOT_R4,   MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "nz-analyzer",   "wss://meshcore-mqtt-1.baird.io:443",      "meshcore-mqtt-1.baird.io",        GTS_ROOT_R4,   MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "meshmapper",    "wss://mqtt.meshmapper.net:443/mqtt",       "mqtt.meshmapper.net",              ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "meshrank",      "mqtts://meshrank.net:8883",               NULL,                           ISRG_ROOT_X1,  MQTT_AUTH_NONE,     MQTT_TOPIC_MESHRANK,  0,       false,  0,       NULL,     NULL     },
    // waev token_lifetime is 3300 (55 min) on purpose: the broker's real JWT TTL is
    // 60 min, and claiming less keeps fresh tokens accepted even with ~5 min of fast
    // device-clock skew (and off any exp-iat<=3600 boundary strictness). Do NOT
    // "fix" this to 3600 — the renewal race is handled separately by
    // tokenRenewalBufferSecs() in MQTTBridge, which renews another 5 min earlier.
    { "waev",          "wss://mqtt.waev.app:443/mqtt",            "mqtt.waev.app",                   GTS_ROOT_R4,   MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE, 3300,     false,  55,      NULL,     NULL     },
    { "meshomatic",    "wss://us-east.meshomatic.net:443/mqtt",   "us-east.meshomatic.net",          ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "cascadiamesh",  "wss://mqtt-v1.cascadiamesh.org:443/mqtt", "mqtt-v1.cascadiamesh.org",        ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "tennmesh",      "mqtt://mqtt.tennmesh.com:1883",           NULL,                           NULL,       MQTT_AUTH_USERPASS,  MQTT_TOPIC_MESHCORE,  0,       true,   55,      "mqttfeed",   "tc2live"   },
    { "nashmesh",      "mqtt://mqtt.nashme.sh:1883",              NULL,                           NULL,       MQTT_AUTH_USERPASS,  MQTT_TOPIC_MESHCORE,  0,       true,   55,      "meshdev",    "large4cats"},
    { "ctmesh",        "mqtt://mqtt.ctmesh.org:1883",             NULL,                           NULL,       MQTT_AUTH_USERPASS,  MQTT_TOPIC_MESHCORE,  0,       true,   60,      "meshdev",    "large4cats"},
    { "chimesh",       "wss://mqtt.chimesh.org:443",              "mqtt.chimesh.org",                ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "meshat.se",     "wss://meshcore-mqtt.meshat.se:443",       "meshcore-mqtt.meshat.se",         ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "eastidahomesh", "mqtt://live.eastidahomesh.com:1883",      NULL,                           NULL,       MQTT_AUTH_NONE,     MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "coloradomesh",  "wss://mqtt.meshcore.coloradomesh.org:443","mqtt.meshcore.coloradomesh.org", ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "dutchmeshcore-1", "wss://collector1.dutchmeshcore.nl:443/mqtt", "collector1.dutchmeshcore.nl",     GTS_ROOT_R4,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "dutchmeshcore-2", "wss://collector2.dutchmeshcore.nl:443/mqtt", "collector2.dutchmeshcore.nl",     GTS_ROOT_R4,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "meshcore-ca-1",   "wss://mqtt1.meshcore.ca:443/mqtt",          "mqtt1.meshcore.ca",               ISRG_ROOT_X1, MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "meshcore-ca-2",   "wss://mqtt2.meshcore.ca:443/mqtt",          "mqtt2.meshcore.ca",               ISRG_ROOT_X1, MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "meshcore-fi",     "wss://mc-mqtt.meshcore.fi:443/",            "mc-mqtt.meshcore.fi",             ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "okimesh-1",     "wss://mqtt1.okimesh.org:9002/mqtt",       "mqtt1.okimesh.org",               ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "okimesh-2",     "wss://mqtt2.okimesh.org:9002/mqtt",       "mqtt2.okimesh.org",               ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "inwmesh",         "mqtts://scope.inwmesh.org:8883",            NULL,                           ISRG_ROOT_X1,  MQTT_AUTH_USERPASS,  MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "bostonmesh",      "wss://mqttmc01.bostonme.sh:443/mqtt",       "mqttmc01.bostonme.sh",            GTS_ROOT_R4,   MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "rflab",           "wss://mqtt.rflab.io:443",                   "mqtt.rflab.io",                   ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "ipnt.uk",         "wss://mqtt.ipnt.uk:443",                    "mqtt.ipnt.uk",                    ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "flmesh",       "wss://mcmqtt.jntconnections.com:443",       "mcmqtt.jntconnections.com",       GTS_ROOT_R4,   MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "corecomms",     "wss://mqtt.corecomms.net:443/mqtt",       "mqtt.corecomms.net",              GTS_ROOT_R4,   MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "meshtexas",     "wss://mqtt.meshtexas.org:443/mqtt",       "mqtt.meshtexas.org",              GTS_ROOT_R4,   MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    // Username is device pubkey hex at connect; password from mqttN.password. No TLS.
    { "mesh-chaun14",  "mqtt://mqtt.mesh.chaun14.fr:1884",        NULL,                           NULL,       MQTT_AUTH_USERPASS,  MQTT_TOPIC_MESHCORE,  0,       true,   60,      MQTT_USERPASS_USERNAME_PUBKEY, NULL },
    // LetsMesh-compatible JWT; TLS is Let's Encrypt (ISRG Root X1), not GTS.
    { "wcmesh",        "wss://mqtt.wcmesh.com:443",               "mqtt.wcmesh.com",                 ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "atvirastinklas","wss://mqtt-mc.atvirastinklas.lt:443",      "mqtt-mc.atvirastinklas.lt",       GTS_ROOT_R4,   MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    // JWT token auth; LE Gen-Y ECDSA chain (YE2 → Root YE → X2) still anchors at ISRG Root X1.
    { "gomesh",        "wss://mqtt.gomesh.dev:443",               "mqtt.gomesh.dev",                 ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "idahomesh",     "wss://mqtt.idahomesh.org:443/mqtt",       "mqtt.idahomesh.org",              ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "ntxmesh",       "wss://ntxmesh.dhovin.me:8883",            "ntxmesh.dhovin.me",               ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "bsmesh",        "wss://mqtt.bsmesh.de:8885",               "mqtt.bsmesh.de",                  ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    { "marwoj",        "mqtts://mqtt.marwoj.net:8883",            NULL,                           ISRG_ROOT_X1,  MQTT_AUTH_USERPASS,  MQTT_TOPIC_MESHCORE,  0,       true,   55,      "observer-agessaman", "ipRwCEclZkX47K" },
    // LE Gen-Y ECDSA chain (YE1 → Root YE → X2) anchors at ISRG Root X1.
    { "sacmesh",       "wss://mqtt.sacmesh.org:443/",             "mqtt.sacmesh.org",                ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    // Upstate Mesh (NY Capital District). JWT; LE Gen-Y ECDSA chain (YE1 → Root YE → X2) anchors at ISRG Root X1.
    { "upstatemesh",   "wss://mqtt.upst8me.sh:443/mqtt",          "mqtt.upst8me.sh",                 ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    // Comchan (Central Texas). JWT; LE Gen-Y RSA chain (YR1 → Root YR → X1) anchors at ISRG Root X1.
    { "comchan",       "wss://obs.comchan.net:443/",              "obs.comchan.net",                 ISRG_ROOT_X1,  MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    // Virginia Mesh. No auth; LE Gen-Y ECDSA chain (YE2 → Root YE → X2) anchors at ISRG Root X1.
    { "virginiamesh",  "wss://mqtt.virginiamesh.com:8883",       NULL,                           ISRG_ROOT_X1,  MQTT_AUTH_NONE,     MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
    // EU MeshCore Analyzer. JWT; leaf → GTS WE1 → GTS Root R4 (same PEM as GTS_ROOT_R4).
    { "meshcore-analyzer-eu", "wss://mqtt.meshcore-analyzer.eu:443", "mqtt.meshcore-analyzer.eu",     GTS_ROOT_R4,   MQTT_AUTH_JWT,      MQTT_TOPIC_MESHCORE,  0,       true,   55,      NULL,     NULL     },
};


const struct MQTTPresetDef *mqtt_preset_find(const char *name)
{
	for (unsigned i = 0; i < MQTT_PRESET_COUNT; ++i) {
		if (!strcmp(name, MQTT_PRESETS[i].name)) return &MQTT_PRESETS[i];
	}
	return NULL;
}

