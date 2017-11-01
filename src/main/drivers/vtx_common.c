/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Created by jflyper */

#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>

#include "platform.h"
#include "build/debug.h"
#include "io/vtx_common.h"
#include "drivers/vtx_common.h"

#if defined(VTX_COMMON)

vtxDevice_t *vtxDevice = NULL;

#define VTX_PARAM_CYCLE_TIME_US 100000 // 10Hz

typedef enum {
    VTX_PARAM_BANDCHAN = 0,
    VTX_PARAM_POWER,
    VTX_PARAM_COUNT
} vtxScheduleParams_e;

static uint8_t vtxParamScheduleCount;
static uint8_t vtxParamSchedule[VTX_PARAM_COUNT];

void vtxCommonInit(void)
{
    uint8_t index = 0;
    vtxParamSchedule[index++] = VTX_PARAM_BANDCHAN;
    vtxParamSchedule[index++] = VTX_PARAM_POWER;
    vtxParamScheduleCount = index;
}

// Whatever registered last will win

void vtxCommonRegisterDevice(vtxDevice_t *pDevice)
{
    vtxDevice = pDevice;
}

bool vtxCommonDeviceRegistered(void)
{
    return vtxDevice;
}

void vtxCommonProcess(uint32_t currentTimeUs)
{
    if (!vtxDevice)
    return;

    static uint32_t lastCycleTimeUs;
    static uint8_t scheduleIndex;
    uint8_t currentSchedule = vtxParamSchedule[scheduleIndex];

    if (vtxDevice->vTable->process) {
        // Process VTX changes from the parameter group at 10Hz
        if (currentTimeUs > lastCycleTimeUs + VTX_PARAM_CYCLE_TIME_US) {
            const vtxSettingsConfig_t settings = vtxCommonGetSettings();
            switch (currentSchedule)
            {
                case VTX_PARAM_BANDCHAN:
                    if (settings.band) {
                        uint8_t vtxBand;
                        uint8_t vtxChan;
                        if (vtxCommonGetBandAndChannel(&vtxBand, &vtxChan)) {
                            if (settings.band != vtxBand || settings.channel != vtxChan) {
                                vtxCommonSetBandAndChannel(settings.band, settings.channel);
                            }
                        }
#if defined(VTX_SETTINGS_FREQCMD)
                    } else {
                        uint16_t vtxFreq;
                        if (vtxCommonGetFrequency(&vtxFreq)) {
                            if (settings.freq != vtxFreq) {
                                vtxCommonSetFrequency(settings.freq);
                            }
                        }
#endif
                    }
                    break;
                case VTX_PARAM_POWER: ;
                    uint8_t vtxPower;
                    if (vtxCommonGetPowerIndex(&vtxPower)) {
                        if (settings.power != vtxPower) {
                            vtxCommonSetPowerByIndex(settings.power);
                        }
                    }
                    break;
                default:
                    break;
            }
            lastCycleTimeUs = currentTimeUs;
            scheduleIndex = (scheduleIndex + 1) % vtxParamScheduleCount;
        }
        vtxDevice->vTable->process(currentTimeUs);
    }
}

vtxDevType_e vtxCommonGetDeviceType(void)
{
    if (!vtxDevice || !vtxDevice->vTable->getDeviceType)
        return VTXDEV_UNKNOWN;

    return vtxDevice->vTable->getDeviceType();
}

// band and channel are 1 origin
void vtxCommonSetBandAndChannel(uint8_t band, uint8_t channel)
{
    if ((band <= vtxDevice->capability.bandCount) && (channel <= vtxDevice->capability.channelCount)) {
        if (vtxDevice->vTable->setBandAndChannel) {
            vtxDevice->vTable->setBandAndChannel(band, channel);
        }
    }
}

// index is zero origin, zero = power off completely
void vtxCommonSetPowerByIndex(uint8_t index)
{
    if (index <= vtxDevice->capability.powerCount) {
        if (vtxDevice->vTable->setPowerByIndex) {
            vtxDevice->vTable->setPowerByIndex(index);
        }
    }
}

// on = 1, off = 0
void vtxCommonSetPitMode(uint8_t onoff)
{
    if (vtxDevice->vTable->setPitMode)
        vtxDevice->vTable->setPitMode(onoff);
}

void vtxCommonSetFrequency(uint16_t freq)
{
    if (vtxDevice->vTable->setFrequency) {
        vtxDevice->vTable->setFrequency(freq);
    }
}

bool vtxCommonGetBandAndChannel(uint8_t *pBand, uint8_t *pChannel)
{
    if (vtxDevice->vTable->getBandAndChannel)
        return vtxDevice->vTable->getBandAndChannel(pBand, pChannel);
    else
        return false;
}

bool vtxCommonGetPowerIndex(uint8_t *pIndex)
{
    if (!vtxDevice)
        return false;

    if (vtxDevice->vTable->getPowerIndex)
        return vtxDevice->vTable->getPowerIndex(pIndex);
    else
        return false;
}

bool vtxCommonGetPitMode(uint8_t *pOnOff)
{
    if (!vtxDevice)
        return false;

    if (vtxDevice->vTable->getPitMode)
        return vtxDevice->vTable->getPitMode(pOnOff);
    else
        return false;
}

bool vtxCommonGetFrequency(uint16_t *pFreq)
{
    if (!vtxDevice) {
        return false;
    }
    if (vtxDevice->vTable->getFrequency) {
        return vtxDevice->vTable->getFrequency(pFreq);
    } else {
        return false;
    }
}

bool vtxCommonGetDeviceCapability(vtxDeviceCapability_t *pDeviceCapability)
{
    if (!vtxDevice)
        return false;

    memcpy(pDeviceCapability, &vtxDevice->capability, sizeof(vtxDeviceCapability_t));
    return true;
}

vtxSettingsConfig_t vtxCommonGetSettings(void)
{
    // return pending changes for real-time feedback.
    return (vtxSettingsConfig_t) {
        .band = vtxSettingsConfigMutable()->band,
        .channel = vtxSettingsConfigMutable()->channel,
        .freq = vtxSettingsConfigMutable()->freq,
        .power = vtxSettingsConfigMutable()->power,
    };
}

void vtxCommonUpdateSettings(vtxSettingsConfig_t config)
{
    vtxSettingsConfigMutable()->band = config.band;
    vtxSettingsConfigMutable()->channel = config.channel;
    vtxSettingsConfigMutable()->freq = config.freq;
    vtxSettingsConfigMutable()->power = config.power;
}

#endif
