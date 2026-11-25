// SPDX-License-Identifier: GPL-2.0-only
/* EXODUS read-only Horizon settings adapter. Independent of SDL and Wine.
 * API signatures checked against libnx b156b4774fb2df7b93402169509266da7547fc4c.
 * Every service is optional; never substitute synthetic values on failure. */
#ifndef EXODUS_HORIZON_SETTINGS_H
#define EXODUS_HORIZON_SETTINGS_H
#include <stdio.h>
#include <string.h>

struct exodus_settings {
    char firmware[64], battery[64], network[64], wifi[64];
    char bluetooth[64], brightness[64], performance[64];
};

#ifdef __SWITCH__
#include <switch.h>

static inline void exodus_service_error(char *dst, Result rc)
{
    snprintf(dst, 64, "Unavailable (0x%08lx)", (unsigned long)rc);
}

static inline void exodus_flag(char *dst, Result rc, bool value)
{
    if (R_SUCCEEDED(rc)) snprintf(dst, 64, "%s", value ? "Enabled" : "Disabled");
    else exodus_service_error(dst, rc);
}
#endif

static inline void exodus_read_settings(struct exodus_settings *s)
{
    memset(s, 0, sizeof(*s));
    snprintf(s->firmware, 64, "Unavailable");
    snprintf(s->battery, 64, "Unavailable");
    snprintf(s->network, 64, "Unavailable");
    snprintf(s->wifi, 64, "Unavailable");
    snprintf(s->bluetooth, 64, "Unavailable");
    snprintf(s->brightness, 64, "Unavailable");
    snprintf(s->performance, 64, "Unavailable");
#ifdef __SWITCH__
    Result rc = psmInitialize();
    if (R_SUCCEEDED(rc)) {
        u32 percent = 0;
        rc = psmGetBatteryChargePercentage(&percent);
        if (R_FAILED(rc)) exodus_service_error(s->battery, rc);
        else if (percent <= 100) {
            PsmChargerType charger;
            Result charge_rc = psmGetChargerType(&charger);
            const char *power = R_FAILED(charge_rc) ? "" :
                charger == PsmChargerType_Unconnected ? " (unplugged)" : " (external power)";
            snprintf(s->battery, 64, "%lu%%%s", (unsigned long)percent, power);
        }
        psmExit();
    } else exodus_service_error(s->battery, rc);

    rc = nifmInitialize(NifmServiceType_User);
    if (R_SUCCEEDED(rc)) {
        bool enabled = false;
        rc = nifmIsWirelessCommunicationEnabled(&enabled);
        exodus_flag(s->wifi, rc, enabled);
        NifmInternetConnectionType type;
        NifmInternetConnectionStatus status;
        u32 strength = 0;
        rc = nifmGetInternetConnectionStatus(&type, &strength, &status);
        if (R_FAILED(rc)) exodus_service_error(s->network, rc);
        else if (status == NifmInternetConnectionStatus_Connected) {
            const char *name = type == NifmInternetConnectionType_WiFi ? "Wi-Fi" :
                type == NifmInternetConnectionType_Ethernet ? "Ethernet" : "Unknown transport";
            snprintf(s->network, 64, "%s connected", name);
        } else snprintf(s->network, 64, "Not confirmed connected");
        nifmExit();
    } else {
        exodus_service_error(s->network, rc);
        exodus_service_error(s->wifi, rc);
    }

    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion version;
        rc = setsysGetFirmwareVersion(&version);
        if (R_SUCCEEDED(rc))
            snprintf(s->firmware, 64, "%u.%u.%u", (unsigned)version.major,
                (unsigned)version.minor, (unsigned)version.micro);
        else exodus_service_error(s->firmware, rc);
        bool enabled = false;
        rc = setsysGetBluetoothEnableFlag(&enabled);
        exodus_flag(s->bluetooth, rc, enabled);
        setsysExit();
    } else {
        exodus_service_error(s->firmware, rc);
        exodus_service_error(s->bluetooth, rc);
    }

    rc = lblInitialize();
    if (R_SUCCEEDED(rc)) {
        float brightness = -1.0f;
        rc = lblGetCurrentBrightnessSetting(&brightness);
        if (R_FAILED(rc)) exodus_service_error(s->brightness, rc);
        else if (brightness >= 0.0f && brightness <= 1.0f)
            snprintf(s->brightness, 64, "%u%%", (unsigned)(brightness * 100.0f + 0.5f));
        lblExit();
    } else exodus_service_error(s->brightness, rc);

    rc = apmInitialize();
    if (R_SUCCEEDED(rc)) {
        ApmPerformanceMode mode;
        rc = apmGetPerformanceMode(&mode);
        if (R_FAILED(rc)) exodus_service_error(s->performance, rc);
        else if (mode == ApmPerformanceMode_Normal || mode == ApmPerformanceMode_Boost)
            snprintf(s->performance, 64, "%s", mode == ApmPerformanceMode_Normal ? "Normal" : "Boost");
        apmExit();
    } else exodus_service_error(s->performance, rc);
#endif
}
#endif
