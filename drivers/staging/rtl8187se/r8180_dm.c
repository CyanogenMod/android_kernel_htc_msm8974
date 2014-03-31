#include "r8180_dm.h"
#include "r8180_hw.h"
#include "r8180_93cx6.h"

 
#define RATE_ADAPTIVE_TIMER_PERIOD      300

bool CheckHighPower(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;

	if(!priv->bRegHighPowerMechanism)
		return false;

	if(ieee->state == IEEE80211_LINKED_SCANNING)
		return false;

	return true;
}

void DoTxHighPower(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	u16			HiPwrUpperTh = 0;
	u16			HiPwrLowerTh = 0;
	u8			RSSIHiPwrUpperTh;
	u8			RSSIHiPwrLowerTh;
	u8			u1bTmp;
	char			OfdmTxPwrIdx, CckTxPwrIdx;

	HiPwrUpperTh = priv->RegHiPwrUpperTh;
	HiPwrLowerTh = priv->RegHiPwrLowerTh;

	HiPwrUpperTh = HiPwrUpperTh * 10;
	HiPwrLowerTh = HiPwrLowerTh * 10;
	RSSIHiPwrUpperTh = priv->RegRSSIHiPwrUpperTh;
	RSSIHiPwrLowerTh = priv->RegRSSIHiPwrLowerTh;

	
	OfdmTxPwrIdx  = priv->chtxpwr_ofdm[priv->ieee80211->current_network.channel];
	CckTxPwrIdx  = priv->chtxpwr[priv->ieee80211->current_network.channel];

	if ((priv->UndecoratedSmoothedSS > HiPwrUpperTh) ||
		(priv->bCurCCKPkt && (priv->CurCCKRSSI > RSSIHiPwrUpperTh))) {
		

		priv->bToUpdateTxPwr = true;
		u1bTmp= read_nic_byte(dev, CCK_TXAGC);

		
		if (CckTxPwrIdx == u1bTmp) {
			u1bTmp = (u1bTmp > 16) ? (u1bTmp -16): 0;  
			write_nic_byte(dev, CCK_TXAGC, u1bTmp);

			u1bTmp= read_nic_byte(dev, OFDM_TXAGC);
			u1bTmp = (u1bTmp > 16) ? (u1bTmp -16): 0;  
			write_nic_byte(dev, OFDM_TXAGC, u1bTmp);
		}

	} else if ((priv->UndecoratedSmoothedSS < HiPwrLowerTh) &&
		(!priv->bCurCCKPkt || priv->CurCCKRSSI < RSSIHiPwrLowerTh)) {
		if (priv->bToUpdateTxPwr) {
			priv->bToUpdateTxPwr = false;
			
			u1bTmp= read_nic_byte(dev, CCK_TXAGC);
			if (u1bTmp < CckTxPwrIdx) {
				write_nic_byte(dev, CCK_TXAGC, CckTxPwrIdx);
			}

			u1bTmp= read_nic_byte(dev, OFDM_TXAGC);
			if (u1bTmp < OfdmTxPwrIdx) {
				write_nic_byte(dev, OFDM_TXAGC, OfdmTxPwrIdx);
			}
		}
	}
}


void rtl8180_tx_pw_wq(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ieee80211_device *ieee = container_of(dwork,struct ieee80211_device,tx_pw_wq);
	struct net_device *dev = ieee->dev;

	DoTxHighPower(dev);
}


bool CheckDig(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;

	if (!priv->bDigMechanism)
		return false;

	if (ieee->state != IEEE80211_LINKED)
		return false;

	if ((priv->ieee80211->rate / 5) < 36) 
		return false;
	return true;
}
void DIG_Zebra(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	u16			CCKFalseAlarm, OFDMFalseAlarm;
	u16			OfdmFA1, OfdmFA2;
	int			InitialGainStep = 7; 
	int			LowestGainStage = 4; 
	u32			AwakePeriodIn2Sec = 0;

	CCKFalseAlarm = (u16)(priv->FalseAlarmRegValue & 0x0000ffff);
	OFDMFalseAlarm = (u16)((priv->FalseAlarmRegValue >> 16) & 0x0000ffff);
	OfdmFA1 =  0x15;
	OfdmFA2 = ((u16)(priv->RegDigOfdmFaUpTh)) << 8;

	
	if (priv->InitialGain == 0) { 
		
		priv->InitialGain = 4; 
	}
	
	OfdmFA1 = 0x20;

#if 1 
	AwakePeriodIn2Sec = (2000 - priv->DozePeriodInPast2Sec);
	priv ->DozePeriodInPast2Sec = 0;

	if (AwakePeriodIn2Sec) {
		OfdmFA1 = (u16)((OfdmFA1 * AwakePeriodIn2Sec) / 2000) ;
		OfdmFA2 = (u16)((OfdmFA2 * AwakePeriodIn2Sec) / 2000) ;
	} else {
		;
	}
#endif

	InitialGainStep = 8;
	LowestGainStage = priv->RegBModeGainStage; 

	if (OFDMFalseAlarm > OfdmFA1) {
		if (OFDMFalseAlarm > OfdmFA2) {
			priv->DIG_NumberFallbackVote++;
			if (priv->DIG_NumberFallbackVote > 1) {
				
				if (priv->InitialGain < InitialGainStep) {
					priv->InitialGainBackUp = priv->InitialGain;

					priv->InitialGain = (priv->InitialGain + 1);
					UpdateInitialGain(dev);
				}
				priv->DIG_NumberFallbackVote = 0;
				priv->DIG_NumberUpgradeVote = 0;
			}
		} else {
			if (priv->DIG_NumberFallbackVote)
				priv->DIG_NumberFallbackVote--;
		}
		priv->DIG_NumberUpgradeVote = 0;
	} else {
		if (priv->DIG_NumberFallbackVote)
			priv->DIG_NumberFallbackVote--;
		priv->DIG_NumberUpgradeVote++;

		if (priv->DIG_NumberUpgradeVote > 9) {
			if (priv->InitialGain > LowestGainStage) { 
				priv->InitialGainBackUp = priv->InitialGain;

				priv->InitialGain = (priv->InitialGain - 1);
				UpdateInitialGain(dev);
			}
			priv->DIG_NumberFallbackVote = 0;
			priv->DIG_NumberUpgradeVote = 0;
		}
	}
}

void DynamicInitGain(struct net_device *dev)
{
	DIG_Zebra(dev);
}

void rtl8180_hw_dig_wq(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ieee80211_device *ieee = container_of(dwork,struct ieee80211_device,hw_dig_wq);
	struct net_device *dev = ieee->dev;
	struct r8180_priv *priv = ieee80211_priv(dev);

	
	priv->FalseAlarmRegValue = read_nic_dword(dev, CCK_FALSE_ALARM);


	
	DynamicInitGain(dev);

}

int IncludedInSupportedRates(struct r8180_priv *priv, u8 TxRate)
{
	u8 rate_len;
	u8 rate_ex_len;
	u8                      RateMask = 0x7F;
	u8                      idx;
	unsigned short          Found = 0;
	u8                      NaiveTxRate = TxRate&RateMask;

	rate_len = priv->ieee80211->current_network.rates_len;
	rate_ex_len = priv->ieee80211->current_network.rates_ex_len;
	for (idx=0; idx < rate_len; idx++) {
		if ((priv->ieee80211->current_network.rates[idx] & RateMask) == NaiveTxRate) {
			Found = 1;
			goto found_rate;
		}
	}
	for (idx = 0; idx < rate_ex_len; idx++) {
		if ((priv->ieee80211->current_network.rates_ex[idx] & RateMask) == NaiveTxRate) {
			Found = 1;
			goto found_rate;
		}
	}
	return Found;
	found_rate:
	return Found;
}

u8 GetUpgradeTxRate(struct net_device *dev, u8 rate)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	u8                      UpRate;

	
	switch (rate) {
	case 108: 
		UpRate = 108;
		break;

	case 96: 
		UpRate = 108;
		break;

	case 72: 
		UpRate = 96;
		break;

	case 48: 
		UpRate = 72;
		break;

	case 36: 
		UpRate = 48;
		break;

	case 22: 
		UpRate = 36;
		break;

	case 11: 
		UpRate = 22;
		break;

	case 4: 
		UpRate = 11;
		break;

	case 2: 
		UpRate = 4;
		break;

	default:
		printk("GetUpgradeTxRate(): Input Tx Rate(%d) is undefined!\n", rate);
		return rate;
	}
	
	if (IncludedInSupportedRates(priv, UpRate)) {
		return UpRate;
	} else {
		return rate;
	}
	return rate;
}

u8 GetDegradeTxRate(struct net_device *dev, u8 rate)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	u8                      DownRate;

	
	switch (rate) {
	case 108: 
		DownRate = 96;
		break;

	case 96: 
		DownRate = 72;
		break;

	case 72: 
		DownRate = 48;
		break;

	case 48: 
		DownRate = 36;
		break;

	case 36: 
		DownRate = 22;
		break;

	case 22: 
		DownRate = 11;
		break;

	case 11: 
		DownRate = 4;
		break;

	case 4: 
		DownRate = 2;
		break;

	case 2: 
		DownRate = 2;
		break;

	default:
		printk("GetDegradeTxRate(): Input Tx Rate(%d) is undefined!\n", rate);
		return rate;
	}
	
	if (IncludedInSupportedRates(priv, DownRate)) {
		return DownRate;
	} else {
		return rate;
	}
	return rate;
}

bool MgntIsCckRate(u16 rate)
{
	bool bReturn = false;

	if ((rate <= 22) && (rate != 12) && (rate != 18)) {
		bReturn = true;
	}

	return bReturn;
}
void TxPwrTracking87SE(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	u8	tmpu1Byte, CurrentThermal, Idx;
	char	CckTxPwrIdx, OfdmTxPwrIdx;

	tmpu1Byte = read_nic_byte(dev, EN_LPF_CAL);
	CurrentThermal = (tmpu1Byte & 0xf0) >> 4; 
	CurrentThermal = (CurrentThermal > 0x0c) ? 0x0c:CurrentThermal;

	if (CurrentThermal != priv->ThermalMeter) {
		
		for (Idx = 1; Idx < 15; Idx++) {
			CckTxPwrIdx = priv->chtxpwr[Idx];
			OfdmTxPwrIdx = priv->chtxpwr_ofdm[Idx];

			if (CurrentThermal > priv->ThermalMeter) {
				
				CckTxPwrIdx += (CurrentThermal - priv->ThermalMeter) * 2;
				OfdmTxPwrIdx += (CurrentThermal - priv->ThermalMeter) * 2;

				if (CckTxPwrIdx > 35)
					CckTxPwrIdx = 35; 
				if (OfdmTxPwrIdx > 35)
					OfdmTxPwrIdx = 35;
			} else {
				
				CckTxPwrIdx -= (priv->ThermalMeter - CurrentThermal) * 2;
				OfdmTxPwrIdx -= (priv->ThermalMeter - CurrentThermal) * 2;

				if (CckTxPwrIdx < 0)
					CckTxPwrIdx = 0;
				if (OfdmTxPwrIdx < 0)
					OfdmTxPwrIdx = 0;
			}

			
			priv->chtxpwr[Idx] = CckTxPwrIdx;
			priv->chtxpwr_ofdm[Idx] = OfdmTxPwrIdx;
		}

		
		rtl8225z2_SetTXPowerLevel(dev, priv->ieee80211->current_network.channel);
	}
	priv->ThermalMeter = CurrentThermal;
}
void StaRateAdaptive87SE(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	unsigned long	CurrTxokCnt;
	u16		CurrRetryCnt;
	u16		CurrRetryRate;
	unsigned long	CurrRxokCnt;
	bool		bTryUp = false;
	bool		bTryDown = false;
	u8		TryUpTh = 1;
	u8		TryDownTh = 2;
	u32		TxThroughput;
	long		CurrSignalStrength;
	bool		bUpdateInitialGain = false;
	u8		u1bOfdm = 0, u1bCck = 0;
	char		OfdmTxPwrIdx, CckTxPwrIdx;

	priv->RateAdaptivePeriod = RATE_ADAPTIVE_TIMER_PERIOD;


	CurrRetryCnt	= priv->CurrRetryCnt;
	CurrTxokCnt	= priv->NumTxOkTotal - priv->LastTxokCnt;
	CurrRxokCnt	= priv->ieee80211->NumRxOkTotal - priv->LastRxokCnt;
	CurrSignalStrength = priv->Stats_RecvSignalPower;
	TxThroughput = (u32)(priv->NumTxOkBytesTotal - priv->LastTxOKBytes);
	priv->LastTxOKBytes = priv->NumTxOkBytesTotal;
	priv->CurrentOperaRate = priv->ieee80211->rate / 5;
	
	if (CurrTxokCnt > 0) {
		CurrRetryRate = (u16)(CurrRetryCnt * 100 / CurrTxokCnt);
	} else {
	
		CurrRetryRate = (u16)(CurrRetryCnt * 100 / 1);
	}

	priv->LastRetryCnt = priv->CurrRetryCnt;
	priv->LastTxokCnt = priv->NumTxOkTotal;
	priv->LastRxokCnt = priv->ieee80211->NumRxOkTotal;
	priv->CurrRetryCnt = 0;

	
	if (CurrRetryRate == 0 && CurrTxokCnt == 0) {
		priv->TryupingCountNoData++;

		
		if (priv->TryupingCountNoData > 30) {
			priv->TryupingCountNoData = 0;
			priv->CurrentOperaRate = GetUpgradeTxRate(dev, priv->CurrentOperaRate);
			
			priv->LastFailTxRate = 0;
			priv->LastFailTxRateSS = -200;
			priv->FailTxRateCount = 0;
		}
		goto SetInitialGain;
	} else {
		priv->TryupingCountNoData = 0; 
	}



	if (priv->CurrentOperaRate == 22 || priv->CurrentOperaRate == 72)
		TryUpTh += 9;
	if (MgntIsCckRate(priv->CurrentOperaRate) || priv->CurrentOperaRate == 36)
		TryDownTh += 1;

	
	if (priv->bTryuping == true) {

		
		if ((CurrRetryRate > 25) && TxThroughput < priv->LastTxThroughput) {
			
			bTryDown = true;
		} else {
			priv->bTryuping = false;
		}
	} else if (CurrSignalStrength > -47 && (CurrRetryRate < 50)) {
		
		if (priv->CurrentOperaRate != priv->ieee80211->current_network.HighestOperaRate) {
			bTryUp = true;
			
			priv->TryupingCount += TryUpTh;
		}

	} else if (CurrTxokCnt > 9 && CurrTxokCnt < 100 && CurrRetryRate >= 600) {
		bTryDown = true;
		
		priv->TryDownCountLowData += TryDownTh;
	} else if (priv->CurrentOperaRate == 108) {
		
		
		if ((CurrRetryRate > 26) && (priv->LastRetryRate > 25)) {
			bTryDown = true;
		}
		
		else if ((CurrRetryRate > 17) && (priv->LastRetryRate > 16) && (CurrSignalStrength > -72)) {
			bTryDown = true;
		}

		if (bTryDown && (CurrSignalStrength < -75)) 
			priv->TryDownCountLowData += TryDownTh;
	}
	else if (priv->CurrentOperaRate == 96) {
		
		
		if (((CurrRetryRate > 48) && (priv->LastRetryRate > 47))) {
			bTryDown = true;
		} else if (((CurrRetryRate > 21) && (priv->LastRetryRate > 20)) && (CurrSignalStrength > -74)) { 
			
			bTryDown = true;
		} else if ((CurrRetryRate > (priv->LastRetryRate + 50)) && (priv->FailTxRateCount > 2)) {
			bTryDown = true;
			priv->TryDownCountLowData += TryDownTh;
		} else if ((CurrRetryRate < 8) && (priv->LastRetryRate < 8)) { 
			bTryUp = true;
		}

		if (bTryDown && (CurrSignalStrength < -75)){
			priv->TryDownCountLowData += TryDownTh;
		}
	} else if (priv->CurrentOperaRate == 72) {
		
		if ((CurrRetryRate > 43) && (priv->LastRetryRate > 41)) {
			
			bTryDown = true;
		} else if ((CurrRetryRate > (priv->LastRetryRate + 50)) && (priv->FailTxRateCount > 2)) {
			bTryDown = true;
			priv->TryDownCountLowData += TryDownTh;
		} else if ((CurrRetryRate < 15) &&  (priv->LastRetryRate < 16)) { 
			bTryUp = true;
		}

		if (bTryDown && (CurrSignalStrength < -80))
			priv->TryDownCountLowData += TryDownTh;

	} else if (priv->CurrentOperaRate == 48) {
		
		
		if (((CurrRetryRate > 63) && (priv->LastRetryRate > 62))) {
			bTryDown = true;
		} else if (((CurrRetryRate > 33) && (priv->LastRetryRate > 32)) && (CurrSignalStrength > -82)) { 
			bTryDown = true;
		} else if ((CurrRetryRate > (priv->LastRetryRate + 50)) && (priv->FailTxRateCount > 2 )) {
			bTryDown = true;
			priv->TryDownCountLowData += TryDownTh;
		} else if ((CurrRetryRate < 20) && (priv->LastRetryRate < 21)) { 
			bTryUp = true;
		}

		if (bTryDown && (CurrSignalStrength < -82))
			priv->TryDownCountLowData += TryDownTh;

	} else if (priv->CurrentOperaRate == 36) {
		if (((CurrRetryRate > 85) && (priv->LastRetryRate > 86))) {
			bTryDown = true;
		} else if ((CurrRetryRate > (priv->LastRetryRate + 50)) && (priv->FailTxRateCount > 2)) {
			bTryDown = true;
			priv->TryDownCountLowData += TryDownTh;
		} else if ((CurrRetryRate < 22) && (priv->LastRetryRate < 23)) { 
			bTryUp = true;
		}
	} else if (priv->CurrentOperaRate == 22) {
		
		if (CurrRetryRate > 95) {
			bTryDown = true;
		}
		else if ((CurrRetryRate < 29) && (priv->LastRetryRate < 30)) { 
			bTryUp = true;
		}
	} else if (priv->CurrentOperaRate == 11) {
		
		if (CurrRetryRate > 149) {
			bTryDown = true;
		} else if ((CurrRetryRate < 60) && (priv->LastRetryRate < 65)) {
			bTryUp = true;
		}
	} else if (priv->CurrentOperaRate == 4) {
		
		if ((CurrRetryRate > 99) && (priv->LastRetryRate > 99)) {
			bTryDown = true;
		} else if ((CurrRetryRate < 65) && (priv->LastRetryRate < 70)) {
			bTryUp = true;
		}
	} else if (priv->CurrentOperaRate == 2) {
		
		if ((CurrRetryRate < 70) && (priv->LastRetryRate < 75)) {
			bTryUp = true;
		}
	}

	if (bTryUp && bTryDown)
	printk("StaRateAdaptive87B(): Tx Rate tried upping and downing simultaneously!\n");

 
	if (!bTryUp && !bTryDown && (priv->TryupingCount == 0) && (priv->TryDownCountLowData == 0)
		&& priv->CurrentOperaRate != priv->ieee80211->current_network.HighestOperaRate && priv->FailTxRateCount < 2) {
		if (jiffies % (CurrRetryRate + 101) == 0) {
			bTryUp = true;
			priv->bTryuping = true;
		}
	}

	
	if (bTryUp) {
		priv->TryupingCount++;
		priv->TryDownCountLowData = 0;


		if ((priv->TryupingCount > (TryUpTh + priv->FailTxRateCount * priv->FailTxRateCount)) ||
			(CurrSignalStrength > priv->LastFailTxRateSS) || priv->bTryuping) {
			priv->TryupingCount = 0;
			if (priv->CurrentOperaRate == 22)
				bUpdateInitialGain = true;

			if (((priv->CurrentOperaRate == 72) || (priv->CurrentOperaRate == 48) || (priv->CurrentOperaRate == 36)) &&
				(priv->FailTxRateCount > 2))
				priv->RateAdaptivePeriod = (RATE_ADAPTIVE_TIMER_PERIOD / 2);

			
			

			priv->CurrentOperaRate = GetUpgradeTxRate(dev, priv->CurrentOperaRate);

			if (priv->CurrentOperaRate == 36) {
				priv->bUpdateARFR = true;
				write_nic_word(dev, ARFR, 0x0F8F); 
			} else if(priv->bUpdateARFR) {
				priv->bUpdateARFR = false;
				write_nic_word(dev, ARFR, 0x0FFF); 
			}

			
			if (priv->LastFailTxRate != priv->CurrentOperaRate) {
				priv->LastFailTxRate = priv->CurrentOperaRate;
				priv->FailTxRateCount = 0;
				priv->LastFailTxRateSS = -200; 
			}
		}
	} else {
		if (priv->TryupingCount > 0)
			priv->TryupingCount --;
	}

	if (bTryDown) {
		priv->TryDownCountLowData++;
		priv->TryupingCount = 0;

		
		if (priv->TryDownCountLowData > TryDownTh || priv->bTryuping) {
			priv->TryDownCountLowData = 0;
			priv->bTryuping = false;
			
			if (priv->LastFailTxRate == priv->CurrentOperaRate) {
				priv->FailTxRateCount++;
				
				if (CurrSignalStrength > priv->LastFailTxRateSS)
					priv->LastFailTxRateSS = CurrSignalStrength;
			} else {
				priv->LastFailTxRate = priv->CurrentOperaRate;
				priv->FailTxRateCount = 1;
				priv->LastFailTxRateSS = CurrSignalStrength;
			}
			priv->CurrentOperaRate = GetDegradeTxRate(dev, priv->CurrentOperaRate);

			
			if ((CurrSignalStrength < -80) && (priv->CurrentOperaRate > 72 )) {
				priv->CurrentOperaRate = 72;
			}

			if (priv->CurrentOperaRate == 36) {
				priv->bUpdateARFR = true;
				write_nic_word(dev, ARFR, 0x0F8F); 
			} else if (priv->bUpdateARFR) {
				priv->bUpdateARFR = false;
				write_nic_word(dev, ARFR, 0x0FFF); 
			}

			if (MgntIsCckRate(priv->CurrentOperaRate)) {
				bUpdateInitialGain = true;
			}
		}
	} else {
		if (priv->TryDownCountLowData > 0)
			priv->TryDownCountLowData--;
	}

	if (priv->FailTxRateCount >= 0x15 ||
		(!bTryUp && !bTryDown && priv->TryDownCountLowData == 0 && priv->TryupingCount && priv->FailTxRateCount > 0x6)) {
		priv->FailTxRateCount--;
	}


	OfdmTxPwrIdx  = priv->chtxpwr_ofdm[priv->ieee80211->current_network.channel];
	CckTxPwrIdx  = priv->chtxpwr[priv->ieee80211->current_network.channel];

	
	if ((priv->CurrentOperaRate < 96) && (priv->CurrentOperaRate > 22)) {
		u1bCck = read_nic_byte(dev, CCK_TXAGC);
		u1bOfdm = read_nic_byte(dev, OFDM_TXAGC);

		
		if (u1bCck == CckTxPwrIdx) {
			if (u1bOfdm != (OfdmTxPwrIdx + 2)) {
			priv->bEnhanceTxPwr = true;
			u1bOfdm = ((u1bOfdm + 2) > 35) ? 35: (u1bOfdm + 2);
			write_nic_byte(dev, OFDM_TXAGC, u1bOfdm);
			}
		} else if (u1bCck < CckTxPwrIdx) {
		
			if (!priv->bEnhanceTxPwr) {
				priv->bEnhanceTxPwr = true;
				u1bOfdm = ((u1bOfdm + 2) > 35) ? 35: (u1bOfdm + 2);
				write_nic_byte(dev, OFDM_TXAGC, u1bOfdm);
			}
		}
	} else if (priv->bEnhanceTxPwr) {  
		u1bCck = read_nic_byte(dev, CCK_TXAGC);
		u1bOfdm = read_nic_byte(dev, OFDM_TXAGC);

		
		if (u1bCck == CckTxPwrIdx) {
			priv->bEnhanceTxPwr = false;
			write_nic_byte(dev, OFDM_TXAGC, OfdmTxPwrIdx);
		}
		
		else if (u1bCck < CckTxPwrIdx) {
			priv->bEnhanceTxPwr = false;
			u1bOfdm = ((u1bOfdm - 2) > 0) ? (u1bOfdm - 2): 0;
			write_nic_byte(dev, OFDM_TXAGC, u1bOfdm);
		}
	}

SetInitialGain:
	if (bUpdateInitialGain) {
		if (MgntIsCckRate(priv->CurrentOperaRate)) { 
			if (priv->InitialGain > priv->RegBModeGainStage) {
				priv->InitialGainBackUp = priv->InitialGain;

				if (CurrSignalStrength < -85) 
					
					priv->InitialGain = priv->RegBModeGainStage;

				else if (priv->InitialGain > priv->RegBModeGainStage + 1)
					priv->InitialGain -= 2;

				else
					priv->InitialGain--;

				printk("StaRateAdaptive87SE(): update init_gain to index %d for date rate %d\n",priv->InitialGain, priv->CurrentOperaRate);
				UpdateInitialGain(dev);
			}
		} else { 
			if (priv->InitialGain < 4) {
				priv->InitialGainBackUp = priv->InitialGain;

				priv->InitialGain++;
				printk("StaRateAdaptive87SE(): update init_gain to index %d for date rate %d\n",priv->InitialGain, priv->CurrentOperaRate);
				UpdateInitialGain(dev);
			}
		}
	}

	
	priv->LastRetryRate = CurrRetryRate;
	priv->LastTxThroughput = TxThroughput;
	priv->ieee80211->rate = priv->CurrentOperaRate * 5;
}

void rtl8180_rate_adapter(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ieee80211_device *ieee = container_of(dwork, struct ieee80211_device, rate_adapter_wq);
	struct net_device *dev = ieee->dev;
	StaRateAdaptive87SE(dev);
}
void timer_rate_adaptive(unsigned long data)
{
	struct r8180_priv *priv = ieee80211_priv((struct net_device *)data);
	if (!priv->up) {
		return;
	}
	if ((priv->ieee80211->iw_mode != IW_MODE_MASTER)
			&& (priv->ieee80211->state == IEEE80211_LINKED) &&
			(priv->ForcedDataRate == 0)) {
		queue_work(priv->ieee80211->wq, (void *)&priv->ieee80211->rate_adapter_wq);
	}
	priv->rateadapter_timer.expires = jiffies + MSECS(priv->RateAdaptivePeriod);
	add_timer(&priv->rateadapter_timer);
}

void SwAntennaDiversityRxOk8185(struct net_device *dev, u8 SignalStrength)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	priv->AdRxOkCnt++;

	if (priv->AdRxSignalStrength != -1) {
		priv->AdRxSignalStrength = ((priv->AdRxSignalStrength * 7) + (SignalStrength * 3)) / 10;
	} else { 
		priv->AdRxSignalStrength = SignalStrength;
	}
	
	if (priv->LastRxPktAntenna) 
		priv->AdMainAntennaRxOkCnt++;
	else	 
		priv->AdAuxAntennaRxOkCnt++;
}
 
bool SetAntenna8185(struct net_device *dev, u8 u1bAntennaIndex)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	bool bAntennaSwitched = false;

	switch (u1bAntennaIndex) {
	case 0:
		
		write_nic_byte(dev, ANTSEL, 0x03);
		
		write_phy_cck(dev, 0x11, 0x9b); 
		write_phy_ofdm(dev, 0x0d, 0x5c); 

		bAntennaSwitched = true;
		break;

	case 1:
		
		write_nic_byte(dev, ANTSEL, 0x00);
		
		write_phy_cck(dev, 0x11, 0xbb); 
		write_phy_ofdm(dev, 0x0d, 0x54); 

		bAntennaSwitched = true;

		break;

	default:
		printk("SetAntenna8185: unknown u1bAntennaIndex(%d)\n", u1bAntennaIndex);
		break;
	}

	if(bAntennaSwitched)
		priv->CurrAntennaIndex = u1bAntennaIndex;

	return bAntennaSwitched;
}
 
bool SwitchAntenna(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	bool		bResult;

	if (priv->CurrAntennaIndex == 0) {
		bResult = SetAntenna8185(dev, 1);
	} else {
		bResult = SetAntenna8185(dev, 0);
	}

	return bResult;
}
void SwAntennaDiversity(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	bool   bSwCheckSS = false;
	if (bSwCheckSS) {
		priv->AdTickCount++;

		printk("(1) AdTickCount: %d, AdCheckPeriod: %d\n",
			priv->AdTickCount, priv->AdCheckPeriod);
		printk("(2) AdRxSignalStrength: %ld, AdRxSsThreshold: %ld\n",
			priv->AdRxSignalStrength, priv->AdRxSsThreshold);
	}

	
	if (priv->ieee80211->state != IEEE80211_LINKED) {
		priv->bAdSwitchedChecking = false;
		
		SwitchAntenna(dev);

	  
	} else if (priv->AdRxOkCnt == 0) {
		priv->bAdSwitchedChecking = false;
		SwitchAntenna(dev);

	  
	} else if (priv->bAdSwitchedChecking == true) {
		priv->bAdSwitchedChecking = false;

		
		priv->AdRxSsThreshold = (priv->AdRxSignalStrength + priv->AdRxSsBeforeSwitched) / 2;

		priv->AdRxSsThreshold = (priv->AdRxSsThreshold > priv->AdMaxRxSsThreshold) ?
					priv->AdMaxRxSsThreshold: priv->AdRxSsThreshold;
		if(priv->AdRxSignalStrength < priv->AdRxSsBeforeSwitched) {
		
			
			priv->AdCheckPeriod *= 2;
			
			if (priv->AdCheckPeriod > priv->AdMaxCheckPeriod)
				priv->AdCheckPeriod = priv->AdMaxCheckPeriod;

			
			SwitchAntenna(dev);
		} else {
		

			
			priv->AdCheckPeriod = priv->AdMinCheckPeriod;
		}

	}
	
	
	else {
		priv->AdTickCount = 0;


		if ((priv->AdMainAntennaRxOkCnt < priv->AdAuxAntennaRxOkCnt)
			&& (priv->CurrAntennaIndex == 0)) {
		

			
			SwitchAntenna(dev);
			priv->bHWAdSwitched = true;
		} else if ((priv->AdAuxAntennaRxOkCnt < priv->AdMainAntennaRxOkCnt)
			&& (priv->CurrAntennaIndex == 1)) {
		

			
			SwitchAntenna(dev);
			priv->bHWAdSwitched = true;
		} else {
		

			
			priv->bHWAdSwitched = false;
		}
		if ((!priv->bHWAdSwitched) && (bSwCheckSS)) {
			
			if (priv->AdRxSignalStrength < priv->AdRxSsThreshold) {
			
				priv->AdRxSsBeforeSwitched = priv->AdRxSignalStrength;
				priv->bAdSwitchedChecking = true;

				SwitchAntenna(dev);
			} else {
			
				priv->bAdSwitchedChecking = false;
				
				if ((priv->AdRxSignalStrength > (priv->AdRxSsThreshold + 10)) && 
					priv->AdRxSsThreshold <= priv->AdMaxRxSsThreshold) { 

					priv->AdRxSsThreshold = (priv->AdRxSsThreshold + priv->AdRxSignalStrength) / 2;
					priv->AdRxSsThreshold = (priv->AdRxSsThreshold > priv->AdMaxRxSsThreshold) ?
								priv->AdMaxRxSsThreshold: priv->AdRxSsThreshold;
				}

				
				if (priv->AdCheckPeriod > priv->AdMinCheckPeriod)
					priv->AdCheckPeriod /= 2;
			}
		}
	}
	
	priv->AdRxOkCnt = 0;
	priv->AdMainAntennaRxOkCnt = 0;
	priv->AdAuxAntennaRxOkCnt = 0;
}

 
bool CheckTxPwrTracking(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	if (!priv->bTxPowerTrack)
		return false;

	
	if (priv->bToUpdateTxPwr)
		return false;

	return true;
}


 
void SwAntennaDiversityTimerCallback(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	RT_RF_POWER_STATE rtState;

	 
	rtState = priv->eRFPowerState;
	do {
		if (rtState == eRfOff) {
			break;
		} else if (rtState == eRfSleep) {
			
			break;
		}
		SwAntennaDiversity(dev);

	} while (false);

	if (priv->up) {
		priv->SwAntennaDiversityTimer.expires = jiffies + MSECS(ANTENNA_DIVERSITY_TIMER_PERIOD);
		add_timer(&priv->SwAntennaDiversityTimer);
	}
}

