/*
 * Copyright (c) 2002-2006 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2006 Atheros Communications, Inc.
 * All rights reserved.
 *
 * $Id: //depot/sw/branches/sam_hal/ar5312/ar5312_attach.c#9 $
 */
#include "opt_ah.h"

#ifdef AH_SUPPORT_AR5312

#if !defined(AH_SUPPORT_5112) && \
    !defined(AH_SUPPORT_5111) && \
    !defined(AH_SUPPORT_2316) && \
    !defined(AH_SUPPORT_2317)
#error "No 5312 RF support defined"
#endif

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"

#include "ar5312/ar5312.h"
#include "ar5312/ar5312reg.h"
#include "ar5312/ar5312phy.h"

/*
 * TODO: Need to talk to Praveen about this, these are
 * not valid 2.4 channels, either we change these
 * or I need to change the beanie coding to accept these
 */
static const u_int16_t channels11b[] = { 2412, 2447, 2484 };
static const u_int16_t channels11g[] = { 2312, 2412, 2484 };

static  HAL_BOOL ar5312GetMacAddr(struct ath_hal *ah);

static void
ar5312AniSetup(struct ath_hal *ah)
{
	static const struct ar5212AniParams aniparams = {
		.maxNoiseImmunityLevel	= 4,	/* levels 0..4 */
		.totalSizeDesired	= { -34, -41, -48, -55, -62 },
		.coarseHigh		= { -18, -18, -16, -14, -12 },
		.coarseLow		= { -52, -56, -60, -64, -70 },
		.firpwr			= { -78, -78, -80, -82, -82 },
		.maxSpurImmunityLevel	= 2,	/* NB: depends on chip rev */
		.cycPwrThr1		= { 2, 4, 6, 8, 10, 12, 14, 16 },
		.maxFirstepLevel	= 2,	/* levels 0..2 */
		.firstep		= { 0, 4, 8 },
		.ofdmTrigHigh		= 500,
		.ofdmTrigLow		= 200,
		.cckTrigHigh		= 200,
		.cckTrigLow		= 100,
		.rssiThrHigh		= 40,
		.rssiThrLow		= 7,
		.period			= 100,
	};
	if (AH_PRIVATE(ah)->ah_macVersion < AR_SREV_VERSION_GRIFFIN) {
		struct ar5212AniParams p;
		OS_MEMCPY(&p, &aniparams, sizeof(struct ar5212AniParams));
		p.maxSpurImmunityLevel = 7;	/* Venice and earlier */
		ar5212AniAttach(ah, &p, &p, AH_TRUE);
	} else
		ar5212AniAttach(ah, &aniparams, &aniparams, AH_TRUE);
}

/*
 * Attach for an AR3212 part.
 */
struct ath_hal *
ar5312Attach(u_int16_t devid, HAL_SOFTC sc,
	HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_STATUS *status)
{
	struct ath_hal_5212 *ahp = AH_NULL;
	struct ath_hal *ah;
	u_int i;
	u_int32_t sum, val, eepEndLoc;
	u_int16_t eeval, data16;
	HAL_STATUS ecode;
	HAL_BOOL rfStatus;

	HALDEBUG(AH_NULL, "%s: sc %p st %u sh %p\n",
		 __func__, sc, st, (void*) sh);

	/* NB: memory is returned zero'd */
	ahp = ath_hal_malloc(sizeof (struct ath_hal_5212));
	if (ahp == AH_NULL) {
		HALDEBUG(AH_NULL, "%s: cannot allocate memory for "
			"state block\n", __func__);
		*status = HAL_ENOMEM;
		return AH_NULL;
	}
	ar5212InitState(ahp, devid, sc, st, sh, status);
	ah = &ahp->ah_priv.h;

	/* override 5212 methods for our needs */
	ah->ah_disable			= ar5312Disable;
	ah->ah_reset			= ar5312Reset;
	ah->ah_phyDisable		= ar5312PhyDisable;
	ah->ah_setLedState		= ar5312SetLedState;
	ah->ah_detectCardPresent	= ar5312DetectCardPresent;
	ah->ah_setPowerMode		= ar5312SetPowerMode;
	ah->ah_getPowerMode		= ar5312GetPowerMode;
	ah->ah_isInterruptPending	= ar5312IsInterruptPending;

	ahp->ah_priv.ah_eepromRead	= ar5312EepromRead;
#ifdef AH_SUPPORT_WRITE_EEPROM
	ahp->ah_priv.ah_eepromWrite	= ar5312EepromWrite;
#endif
#if ( AH_SUPPORT_2316 || AH_SUPPORT_2317)
	if (IS_5315(ah)) {
		ahp->ah_priv.ah_gpioCfgOutput	= ar5315GpioCfgOutput;
		ahp->ah_priv.ah_gpioCfgInput	= ar5315GpioCfgInput;
		ahp->ah_priv.ah_gpioGet		= ar5315GpioGet;
		ahp->ah_priv.ah_gpioSet		= ar5315GpioSet;
		ahp->ah_priv.ah_gpioSetIntr	= ar5315GpioSetIntr;
	} else
#endif
	{
		ahp->ah_priv.ah_gpioCfgOutput	= ar5312GpioCfgOutput;
		ahp->ah_priv.ah_gpioCfgInput	= ar5312GpioCfgInput;
		ahp->ah_priv.ah_gpioGet		= ar5312GpioGet;
		ahp->ah_priv.ah_gpioSet		= ar5312GpioSet;
		ahp->ah_priv.ah_gpioSetIntr	= ar5312GpioSetIntr;
	}

	ah->ah_gpioCfgInput		= ahp->ah_priv.ah_gpioCfgInput;
	ah->ah_gpioCfgOutput		= ahp->ah_priv.ah_gpioCfgOutput;
	ah->ah_gpioGet			= ahp->ah_priv.ah_gpioGet;
	ah->ah_gpioSet			= ahp->ah_priv.ah_gpioSet;
	ah->ah_gpioSetIntr		= ahp->ah_priv.ah_gpioSetIntr;

	if (!ar5312ChipReset(ah, AH_NULL)) {	/* reset chip */
		HALDEBUG(ah, "%s: chip reset failed\n", __func__);
		ecode = HAL_EIO;
		goto bad;
	}

	HALDEBUG (AH_NULL," Point B\n");

#if ( AH_SUPPORT_2316 || AH_SUPPORT_2317)
	if ((devid == AR5212_AR2315_REV6) ||
	    (devid == AR5212_AR2315_REV7) ||
	    (devid == AR5212_AR2317_REV1) ||
	    (devid == AR5212_AR2317_REV2) ) {
		val = ((OS_REG_READ(ah, (AR5315_RSTIMER_BASE -((u_int32_t) sh)) + AR5315_WREV)) >> AR5315_WREV_S)
			& AR5315_WREV_ID;
		AH_PRIVATE(ah)->ah_macVersion = val >> AR5315_WREV_ID_S;
		AH_PRIVATE(ah)->ah_macRev = val & AR5315_WREV_REVISION;
		HALDEBUG(ah, "%s: Mac Chip Rev 0x%02x.%x\n" , __func__,
			 AH_PRIVATE(ah)->ah_macVersion,
			 AH_PRIVATE(ah)->ah_macRev);
	}
    else
#endif
	{
		val = OS_REG_READ(ah, (AR5312_RSTIMER_BASE - ((u_int32_t) sh)) + 0x0020);
		val = OS_REG_READ(ah, (AR5312_RSTIMER_BASE - ((u_int32_t) sh)) + 0x0080);
		/* Read Revisions from Chips */
		val = ((OS_REG_READ(ah, (AR5312_RSTIMER_BASE - ((u_int32_t) sh)) + AR5312_WREV)) >> AR5312_WREV_S) & AR5312_WREV_ID;
		AH_PRIVATE(ah)->ah_macVersion = val >> AR5312_WREV_ID_S;
		AH_PRIVATE(ah)->ah_macRev = val & AR5312_WREV_REVISION;
	}
	/* XXX - THIS IS WRONG. NEEDS TO BE FIXED */
	if ( ( (AH_PRIVATE(ah)->ah_macVersion != AR_SREV_VERSION_VENICE &&
              AH_PRIVATE(ah)->ah_macVersion != AR_SREV_VERSION_VENICE) ||
             AH_PRIVATE(ah)->ah_macRev < AR_SREV_D2PLUS) &&
              AH_PRIVATE(ah)->ah_macVersion != AR_SREV_VERSION_COBRA) {
#ifdef AH_DEBUG
		HALDEBUG(ah, "%s: Mac Chip Rev 0x%02x.%x is not supported by "
                         "this driver\n", __func__,
                         AH_PRIVATE(ah)->ah_macVersion,
                         AH_PRIVATE(ah)->ah_macRev);
#endif
		ecode = HAL_ENOTSUPP;
		goto bad;
	}
        
	AH_PRIVATE(ah)->ah_phyRev = OS_REG_READ(ah, AR_PHY_CHIP_ID);
        
	if (!ar5212ChipTest(ah)) {
		HALDEBUG(ah, "%s: hardware self-test failed\n", __func__);
		ecode = HAL_ESELFTEST;
		goto bad;
	}

	/*
	 * Set correct Baseband to analog shift
	 * setting to access analog chips.
	 */
	OS_REG_WRITE(ah, AR_PHY(0), 0x00000007);
        
	/* Read Radio Chip Rev Extract */
	AH_PRIVATE(ah)->ah_analog5GhzRev = ar5212GetRadioRev(ah);
#ifdef AH_DEBUG
	/* NB: silently accept anything in release code per Atheros */
	if ((AH_PRIVATE(ah)->ah_analog5GhzRev & 0xF0) !=
            AR_RAD5111_SREV_MAJOR &&
	    (AH_PRIVATE(ah)->ah_analog5GhzRev & 0xF0) !=
            AR_RAD5112_SREV_MAJOR &&
	    (AH_PRIVATE(ah)->ah_analog5GhzRev & 0xF0) !=
            AR_RAD2111_SREV_MAJOR &&
            (AH_PRIVATE(ah)->ah_analog5GhzRev & 0xF0) !=
            AR_RAD2112_SREV_MAJOR) {
		HALDEBUG(ah, "%s: 5G Radio Chip Rev 0x%02X is not supported by "
                         "this driver\n", __func__,
                         AH_PRIVATE(ah)->ah_analog5GhzRev);
		ecode = HAL_ENOTSUPP;
		goto bad;
	}
#endif
	if (IS_5112(ah) && !IS_RADX112_REV2(ah)) {
#ifdef AH_DEBUG
		HALDEBUG(ah, "%s: 5112 Rev 1 is not supported by this "
                         "driver (analog5GhzRev 0x%x)\n", __func__,
                         AH_PRIVATE(ah)->ah_analog5GhzRev);
#endif
		ecode = HAL_ENOTSUPP;
		goto bad;
	}
        
	if (!ar5312EepromRead(ah, AR_EEPROM_VERSION, &eeval)) {
		HALDEBUG(ah, "%s: unable to read EEPROM version\n", __func__);
		ecode = HAL_EEREAD;
		goto bad;
	}
	if (eeval < AR_EEPROM_VER3_2) {
		HALDEBUG(ah, "%s: unsupported EEPROM version %u (0x%x)\n",
                         __func__, eeval, eeval);
		ecode = HAL_EEVERSION;
		goto bad;
	}
	ahp->ah_eeversion = eeval;

	if (ar5312EepromRead(ah, AR_EEPROM_SUBSYSTEM_ID, &eeval)) {
		AH_PRIVATE(ah)->ah_subsystemid = eeval;
	}
	if (ar5312EepromRead(ah, AR_EEPROM_SUBVENDOR_ID, &eeval)) {
		AH_PRIVATE(ah)->ah_subvendorid = eeval;
	}
        
        /* Read sizing information */
        if (!ar5312EepromRead(ah, AR_EEPROM_SIZE_UPPER, &data16)) {
                HALDEBUG(ah, "%s: cannot read eeprom upper size\n", __func__);
                ecode = HAL_EEREAD;
                goto bad;
        }
        
        if (data16 == 0) {
                eepEndLoc = AR_EEPROM_ATHEROS_MAX_LOC;
        } else {
                eepEndLoc = (data16 & AR_EEPROM_SIZE_UPPER_MASK) << AR_EEPROM_SIZE_UPPER_SHIFT;
                if (!ar5312EepromRead(ah, AR_EEPROM_SIZE_LOWER, &data16)) {
                        HALDEBUG(ah, "%s: cannot read eeprom lower size\n", __func__
                                 );
                        ecode = HAL_EEREAD;
                        goto bad;
                }
                eepEndLoc |= data16;
        }
        

        HALASSERT(eepEndLoc > AR_EEPROM_ATHEROS_BASE);
        
	if (!ar5312EepromRead(ah, AR_EEPROM_PROTECT, &eeval)) {
		HALDEBUG(ah, "%s: cannot read EEPROM protection "
                         "bits; read locked?\n", __func__);
		ecode = HAL_EEREAD;
		goto bad;
	}
	HALDEBUG(ah, "EEPROM protect 0x%x\n", eeval);
	ahp->ah_eeprotect = eeval;
	/* XXX check proper access before continuing */
        
	/*
	 * Read the Atheros EEPROM entries and calculate the checksum.
	 */
	sum = 0;
	for (i = 0; i < AR_EEPROM_ATHEROS_MAX; i++) {
		if (!ar5312EepromRead(ah, AR_EEPROM_ATHEROS(i), &eeval)) {
			HALDEBUG(ah, "Error reading during checksum\n");
			ecode = HAL_EEREAD;
			goto bad;
		}
		sum ^= eeval;
	}
	if (sum != 0xffff) {
		HALDEBUG(ah, "%s: bad EEPROM checksum 0x%x\n", __func__, sum);
//		ecode = HAL_EEBADSUM;
//		goto bad;
	}

	ahp->ah_numChannels11a = NUM_11A_EEPROM_CHANNELS;
	ahp->ah_numChannels2_4 = NUM_2_4_EEPROM_CHANNELS;

	for (i = 0; i < NUM_11A_EEPROM_CHANNELS; i ++)
		ahp->ah_dataPerChannel11a[i].numPcdacValues = NUM_PCDAC_VALUES;

	/* the channel list for 2.4 is fixed, fill this in here */
	for (i = 0; i < NUM_2_4_EEPROM_CHANNELS; i++) {
		ahp->ah_channels11b[i] = channels11b[i];
		ahp->ah_channels11g[i] = channels11g[i];
		ahp->ah_dataPerChannel11b[i].numPcdacValues = NUM_PCDAC_VALUES;
		ahp->ah_dataPerChannel11g[i].numPcdacValues = NUM_PCDAC_VALUES;
	}

	if (!ar5212RefreshCalibration(ah)) {
		ecode = HAL_EEREAD;		/* XXX */
		goto bad;
	}

	/*
	 * If Bmode and AR5212, verify 2.4 analog exists
	 */
	if (ahp->ah_Bmode &&
	    (AH_PRIVATE(ah)->ah_analog5GhzRev & 0xF0) == AR_RAD5111_SREV_MAJOR) {
		/*
		 * Set correct Baseband to analog shift
		 * setting to access analog chips.
		 */
		OS_REG_WRITE(ah, AR_PHY(0), 0x00004007);
		OS_DELAY(2000);
		AH_PRIVATE(ah)->ah_analog2GhzRev = ar5212GetRadioRev(ah);

		/* Set baseband for 5GHz chip */
		OS_REG_WRITE(ah, AR_PHY(0), 0x00000007);
		OS_DELAY(2000);
		if ((AH_PRIVATE(ah)->ah_analog2GhzRev & 0xF0) != AR_RAD2111_SREV_MAJOR) {
#ifdef AH_DEBUG
			HALDEBUG(ah, "%s: 2G Radio Chip Rev 0x%02X is not "
				"supported by this driver\n", __func__,
				AH_PRIVATE(ah)->ah_analog2GhzRev);
#endif
			ecode = HAL_ENOTSUPP;
			goto bad;
		}
	}

/*        if (!ar5312EepromRead(ah, AR_EEPROM_REG_DOMAIN, &eeval)) {
		HALDEBUG(ah, "%s: cannot read regulator domain from EEPROM\n",
			__func__);
		ecode = HAL_EEREAD;
		goto bad;
        }*/
        eeval = 0x10;
	/* XXX record serial number */
	ahp->ah_regdomain = eeval;
	AH_PRIVATE(ah)->ah_currentRD = ahp->ah_regdomain;

	/* XXX other capabilities */
	/*
	 * Got everything we need now to setup the capabilities.
	 */
	if (!ar5212FillCapabilityInfo(ah)) {
		HALDEBUG(ah, "%s:failed ar5212FillCapabilityInfo\n", __func__);
		ecode = HAL_EEREAD;
		goto bad;
	}

	rfStatus = AH_FALSE;
	if (IS_2317(ah))
#if defined AH_SUPPORT_2317
		rfStatus = ar2317RfAttach(ah, &ecode);
#else
		ecode = HAL_ENOTSUPP;
#endif
	else if (IS_2316(ah))
#if defined AH_SUPPORT_2316 
		rfStatus = ar2316RfAttach(ah, &ecode);
#else
		ecode = HAL_ENOTSUPP;
#endif
	else if (IS_5112(ah))
#ifdef AH_SUPPORT_5112
		rfStatus = ar5112RfAttach(ah, &ecode);
#else
		ecode = HAL_ENOTSUPP;
#endif
	else
#ifdef AH_SUPPORT_5111
		rfStatus = ar5111RfAttach(ah, &ecode);
#else
		ecode = HAL_ENOTSUPP;
#endif
	if (!rfStatus) {
		HALDEBUG(ah, "%s: RF setup failed, status %u\n",
			__func__, ecode);
		goto bad;
	}
	/* arrange a direct call instead of thunking */
	AH_PRIVATE(ah)->ah_getNfAdjust = ahp->ah_rfHal.getNfAdjust;

	/* Initialize gain ladder thermal calibration structure */
	ar5212InitializeGainValues(ah);

        /* BSP specific call for MAC address of this WMAC device */
        if (!ar5312GetMacAddr(ah)) {
                ecode = HAL_EEBADMAC;
                goto bad;
        }

	ar5312AniSetup(ah);
	ar5212InitNfCalHistBuffer(ah);

	/* XXX EAR stuff goes here */
	return ah;

bad:
	if (ahp)
		ar5212Detach((struct ath_hal *) ahp);
	if (status)
		*status = ecode;
	return AH_NULL;
}

static HAL_BOOL
ar5312GetMacAddr(struct ath_hal *ah)
{
	const struct ar531x_boarddata *board = AR5312_BOARDCONFIG(ah); 
        int wlanNum = AR5312_UNIT(ah);
        const u_int8_t *macAddr;

	switch (wlanNum) {
	case 0:
		macAddr = board->wlan0Mac;
		break;
	case 1:
		macAddr = board->wlan1Mac;
		break;
	default:
#ifdef AH_DEBUG
		HALDEBUG(ah, "Invalid WLAN wmac index (%d)\n",
			       wlanNum);
#endif
		return AH_FALSE;
	}
	OS_MEMCPY(AH5212(ah)->ah_macaddr, macAddr, 6);
	return AH_TRUE;
}
#endif /* AH_SUPPORT_AR5312 */
