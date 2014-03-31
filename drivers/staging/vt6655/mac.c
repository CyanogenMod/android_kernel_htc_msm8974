/*
 * Copyright (c) 1996, 2003 VIA Networking Technologies, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *
 * File: mac.c
 *
 * Purpose:  MAC routines
 *
 * Author: Tevin Chen
 *
 * Date: May 21, 1996
 *
 * Functions:
 *      MACvReadAllRegs - Read All MAC Registers to buffer
 *      MACbIsRegBitsOn - Test if All test Bits On
 *      MACbIsRegBitsOff - Test if All test Bits Off
 *      MACbIsIntDisable - Test if MAC interrupt disable
 *      MACbyReadMultiAddr - Read Multicast Address Mask Pattern
 *      MACvWriteMultiAddr - Write Multicast Address Mask Pattern
 *      MACvSetMultiAddrByHash - Set Multicast Address Mask by Hash value
 *      MACvResetMultiAddrByHash - Clear Multicast Address Mask by Hash value
 *      MACvSetRxThreshold - Set Rx Threshold value
 *      MACvGetRxThreshold - Get Rx Threshold value
 *      MACvSetTxThreshold - Set Tx Threshold value
 *      MACvGetTxThreshold - Get Tx Threshold value
 *      MACvSetDmaLength - Set Dma Length value
 *      MACvGetDmaLength - Get Dma Length value
 *      MACvSetShortRetryLimit - Set 802.11 Short Retry limit
 *      MACvGetShortRetryLimit - Get 802.11 Short Retry limit
 *      MACvSetLongRetryLimit - Set 802.11 Long Retry limit
 *      MACvGetLongRetryLimit - Get 802.11 Long Retry limit
 *      MACvSetLoopbackMode - Set MAC Loopback Mode
 *      MACbIsInLoopbackMode - Test if MAC in Loopback mode
 *      MACvSetPacketFilter - Set MAC Address Filter
 *      MACvSaveContext - Save Context of MAC Registers
 *      MACvRestoreContext - Restore Context of MAC Registers
 *      MACbCompareContext - Compare if values of MAC Registers same as Context
 *      MACbSoftwareReset - Software Reset MAC
 *      MACbSafeRxOff - Turn Off MAC Rx
 *      MACbSafeTxOff - Turn Off MAC Tx
 *      MACbSafeStop - Stop MAC function
 *      MACbShutdown - Shut down MAC
 *      MACvInitialize - Initialize MAC
 *      MACvSetCurrRxDescAddr - Set Rx Descriptos Address
 *      MACvSetCurrTx0DescAddr - Set Tx0 Descriptos Address
 *      MACvSetCurrTx1DescAddr - Set Tx1 Descriptos Address
 *      MACvTimer0MicroSDelay - Micro Second Delay Loop by MAC
 *
 * Revision History:
 *      08-22-2003 Kyle Hsu     :  Porting MAC functions from sim53
 *      09-03-2003 Bryan YC Fan :  Add MACvClearBusSusInd()& MACvEnableBusSusEn()
 *      09-18-2003 Jerry Chen   :  Add MACvSetKeyEntry & MACvDisableKeyEntry
 *
 */

#include "tmacro.h"
#include "tether.h"
#include "mac.h"

unsigned short TxRate_iwconfig;
static int          msglevel                =MSG_LEVEL_INFO;









void MACvReadAllRegs (unsigned long dwIoBase, unsigned char *pbyMacRegs)
{
    int ii;

    
    for (ii = 0; ii < MAC_MAX_CONTEXT_SIZE_PAGE0; ii++) {
        VNSvInPortB(dwIoBase + ii, pbyMacRegs);
        pbyMacRegs++;
    }

    MACvSelectPage1(dwIoBase);

    
    for (ii = 0; ii < MAC_MAX_CONTEXT_SIZE_PAGE1; ii++) {
        VNSvInPortB(dwIoBase + ii, pbyMacRegs);
        pbyMacRegs++;
    }

    MACvSelectPage0(dwIoBase);

}

bool MACbIsRegBitsOn (unsigned long dwIoBase, unsigned char byRegOfs, unsigned char byTestBits)
{
    unsigned char byData;

    VNSvInPortB(dwIoBase + byRegOfs, &byData);
    return (byData & byTestBits) == byTestBits;
}

bool MACbIsRegBitsOff (unsigned long dwIoBase, unsigned char byRegOfs, unsigned char byTestBits)
{
    unsigned char byData;

    VNSvInPortB(dwIoBase + byRegOfs, &byData);
    return !(byData & byTestBits);
}

bool MACbIsIntDisable (unsigned long dwIoBase)
{
    unsigned long dwData;

    VNSvInPortD(dwIoBase + MAC_REG_IMR, &dwData);
    if (dwData != 0)
        return false;

    return true;
}

unsigned char MACbyReadMultiAddr (unsigned long dwIoBase, unsigned int uByteIdx)
{
    unsigned char byData;

    MACvSelectPage1(dwIoBase);
    VNSvInPortB(dwIoBase + MAC_REG_MAR0 + uByteIdx, &byData);
    MACvSelectPage0(dwIoBase);
    return byData;
}

void MACvWriteMultiAddr (unsigned long dwIoBase, unsigned int uByteIdx, unsigned char byData)
{
    MACvSelectPage1(dwIoBase);
    VNSvOutPortB(dwIoBase + MAC_REG_MAR0 + uByteIdx, byData);
    MACvSelectPage0(dwIoBase);
}

void MACvSetMultiAddrByHash (unsigned long dwIoBase, unsigned char byHashIdx)
{
    unsigned int uByteIdx;
    unsigned char byBitMask;
    unsigned char byOrgValue;

    
    uByteIdx = byHashIdx / 8;
    ASSERT(uByteIdx < 8);
    
    byBitMask = 1;
    byBitMask <<= (byHashIdx % 8);
    
    byOrgValue = MACbyReadMultiAddr(dwIoBase, uByteIdx);
    MACvWriteMultiAddr(dwIoBase, uByteIdx, (unsigned char)(byOrgValue | byBitMask));
}

void MACvResetMultiAddrByHash (unsigned long dwIoBase, unsigned char byHashIdx)
{
    unsigned int uByteIdx;
    unsigned char byBitMask;
    unsigned char byOrgValue;

    
    uByteIdx = byHashIdx / 8;
    ASSERT(uByteIdx < 8);
    
    byBitMask = 1;
    byBitMask <<= (byHashIdx % 8);
    
    byOrgValue = MACbyReadMultiAddr(dwIoBase, uByteIdx);
    MACvWriteMultiAddr(dwIoBase, uByteIdx, (unsigned char)(byOrgValue & (~byBitMask)));
}

void MACvSetRxThreshold (unsigned long dwIoBase, unsigned char byThreshold)
{
    unsigned char byOrgValue;

    ASSERT(byThreshold < 4);

    
    VNSvInPortB(dwIoBase + MAC_REG_FCR0, &byOrgValue);
    byOrgValue = (byOrgValue & 0xCF) | (byThreshold << 4);
    VNSvOutPortB(dwIoBase + MAC_REG_FCR0, byOrgValue);
}

void MACvGetRxThreshold (unsigned long dwIoBase, unsigned char *pbyThreshold)
{
    
    VNSvInPortB(dwIoBase + MAC_REG_FCR0, pbyThreshold);
    *pbyThreshold = (*pbyThreshold >> 4) & 0x03;
}

void MACvSetTxThreshold (unsigned long dwIoBase, unsigned char byThreshold)
{
    unsigned char byOrgValue;

    ASSERT(byThreshold < 4);

    
    VNSvInPortB(dwIoBase + MAC_REG_FCR0, &byOrgValue);
    byOrgValue = (byOrgValue & 0xF3) | (byThreshold << 2);
    VNSvOutPortB(dwIoBase + MAC_REG_FCR0, byOrgValue);
}

void MACvGetTxThreshold (unsigned long dwIoBase, unsigned char *pbyThreshold)
{
    
    VNSvInPortB(dwIoBase + MAC_REG_FCR0, pbyThreshold);
    *pbyThreshold = (*pbyThreshold >> 2) & 0x03;
}

void MACvSetDmaLength (unsigned long dwIoBase, unsigned char byDmaLength)
{
    unsigned char byOrgValue;

    ASSERT(byDmaLength < 4);

    
    VNSvInPortB(dwIoBase + MAC_REG_FCR0, &byOrgValue);
    byOrgValue = (byOrgValue & 0xFC) | byDmaLength;
    VNSvOutPortB(dwIoBase + MAC_REG_FCR0, byOrgValue);
}

void MACvGetDmaLength (unsigned long dwIoBase, unsigned char *pbyDmaLength)
{
    
    VNSvInPortB(dwIoBase + MAC_REG_FCR0, pbyDmaLength);
    *pbyDmaLength &= 0x03;
}

void MACvSetShortRetryLimit (unsigned long dwIoBase, unsigned char byRetryLimit)
{
    
    VNSvOutPortB(dwIoBase + MAC_REG_SRT, byRetryLimit);
}

void MACvGetShortRetryLimit (unsigned long dwIoBase, unsigned char *pbyRetryLimit)
{
    
    VNSvInPortB(dwIoBase + MAC_REG_SRT, pbyRetryLimit);
}

void MACvSetLongRetryLimit (unsigned long dwIoBase, unsigned char byRetryLimit)
{
    
    VNSvOutPortB(dwIoBase + MAC_REG_LRT, byRetryLimit);
}

void MACvGetLongRetryLimit (unsigned long dwIoBase, unsigned char *pbyRetryLimit)
{
    
    VNSvInPortB(dwIoBase + MAC_REG_LRT, pbyRetryLimit);
}

void MACvSetLoopbackMode (unsigned long dwIoBase, unsigned char byLoopbackMode)
{
    unsigned char byOrgValue;

    ASSERT(byLoopbackMode < 3);
    byLoopbackMode <<= 6;
    
    VNSvInPortB(dwIoBase + MAC_REG_TEST, &byOrgValue);
    byOrgValue = byOrgValue & 0x3F;
    byOrgValue = byOrgValue | byLoopbackMode;
    VNSvOutPortB(dwIoBase + MAC_REG_TEST, byOrgValue);
}

bool MACbIsInLoopbackMode (unsigned long dwIoBase)
{
    unsigned char byOrgValue;

    VNSvInPortB(dwIoBase + MAC_REG_TEST, &byOrgValue);
    if (byOrgValue & (TEST_LBINT | TEST_LBEXT))
        return true;
    return false;
}

void MACvSetPacketFilter (unsigned long dwIoBase, unsigned short wFilterType)
{
    unsigned char byOldRCR;
    unsigned char byNewRCR = 0;

    
    
    
    if (wFilterType & PKT_TYPE_DIRECTED) {
        
        MACvSelectPage1(dwIoBase);
        VNSvOutPortD(dwIoBase + MAC_REG_MAR0, 0L);
        VNSvOutPortD(dwIoBase + MAC_REG_MAR0 + sizeof(unsigned long), 0L);
        MACvSelectPage0(dwIoBase);
    }

    if (wFilterType & (PKT_TYPE_PROMISCUOUS | PKT_TYPE_ALL_MULTICAST)) {
        
        MACvSelectPage1(dwIoBase);
        VNSvOutPortD(dwIoBase + MAC_REG_MAR0, 0xFFFFFFFFL);
        VNSvOutPortD(dwIoBase + MAC_REG_MAR0 + sizeof(unsigned long), 0xFFFFFFFFL);
        MACvSelectPage0(dwIoBase);
    }

    if (wFilterType & PKT_TYPE_PROMISCUOUS) {

        byNewRCR |= (RCR_RXALLTYPE | RCR_UNICAST | RCR_MULTICAST | RCR_BROADCAST);

        byNewRCR &= ~RCR_BSSID;
    }

    if (wFilterType & (PKT_TYPE_ALL_MULTICAST | PKT_TYPE_MULTICAST))
        byNewRCR |= RCR_MULTICAST;

    if (wFilterType & PKT_TYPE_BROADCAST)
        byNewRCR |= RCR_BROADCAST;

    if (wFilterType & PKT_TYPE_ERROR_CRC)
        byNewRCR |= RCR_ERRCRC;

    VNSvInPortB(dwIoBase + MAC_REG_RCR,  &byOldRCR);
    if (byNewRCR != byOldRCR) {
        
        VNSvOutPortB(dwIoBase + MAC_REG_RCR, byNewRCR);
    }
}

void MACvSaveContext (unsigned long dwIoBase, unsigned char *pbyCxtBuf)
{
    int         ii;

    
    for (ii = 0; ii < MAC_MAX_CONTEXT_SIZE_PAGE0; ii++) {
        VNSvInPortB((dwIoBase + ii), (pbyCxtBuf + ii));
    }

    MACvSelectPage1(dwIoBase);

    
    for (ii = 0; ii < MAC_MAX_CONTEXT_SIZE_PAGE1; ii++) {
        VNSvInPortB((dwIoBase + ii), (pbyCxtBuf + MAC_MAX_CONTEXT_SIZE_PAGE0 + ii));
    }

    MACvSelectPage0(dwIoBase);
}

void MACvRestoreContext (unsigned long dwIoBase, unsigned char *pbyCxtBuf)
{
    int         ii;

    MACvSelectPage1(dwIoBase);
    
    for (ii = 0; ii < MAC_MAX_CONTEXT_SIZE_PAGE1; ii++) {
        VNSvOutPortB((dwIoBase + ii), *(pbyCxtBuf + MAC_MAX_CONTEXT_SIZE_PAGE0 + ii));
    }
    MACvSelectPage0(dwIoBase);

    
    for (ii = MAC_REG_RCR; ii < MAC_REG_ISR; ii++) {
        VNSvOutPortB(dwIoBase + ii, *(pbyCxtBuf + ii));
    }
    
    for (ii = MAC_REG_LRT; ii < MAC_REG_PAGE1SEL; ii++) {
        VNSvOutPortB(dwIoBase + ii, *(pbyCxtBuf + ii));
    }
    VNSvOutPortB(dwIoBase + MAC_REG_CFG, *(pbyCxtBuf + MAC_REG_CFG));

    
    for (ii = MAC_REG_PSCFG; ii < MAC_REG_BBREGCTL; ii++) {
        VNSvOutPortB(dwIoBase + ii, *(pbyCxtBuf + ii));
    }

    
    VNSvOutPortD(dwIoBase + MAC_REG_TXDMAPTR0, *(unsigned long *)(pbyCxtBuf + MAC_REG_TXDMAPTR0));
    VNSvOutPortD(dwIoBase + MAC_REG_AC0DMAPTR, *(unsigned long *)(pbyCxtBuf + MAC_REG_AC0DMAPTR));
    VNSvOutPortD(dwIoBase + MAC_REG_BCNDMAPTR, *(unsigned long *)(pbyCxtBuf + MAC_REG_BCNDMAPTR));


    VNSvOutPortD(dwIoBase + MAC_REG_RXDMAPTR0, *(unsigned long *)(pbyCxtBuf + MAC_REG_RXDMAPTR0));

    VNSvOutPortD(dwIoBase + MAC_REG_RXDMAPTR1, *(unsigned long *)(pbyCxtBuf + MAC_REG_RXDMAPTR1));

}

bool MACbCompareContext (unsigned long dwIoBase, unsigned char *pbyCxtBuf)
{
    unsigned long dwData;

    
    

    
    VNSvInPortD(dwIoBase + MAC_REG_TXDMAPTR0, &dwData);
    if (dwData != *(unsigned long *)(pbyCxtBuf + MAC_REG_TXDMAPTR0)) {
        return false;
    }

    VNSvInPortD(dwIoBase + MAC_REG_AC0DMAPTR, &dwData);
    if (dwData != *(unsigned long *)(pbyCxtBuf + MAC_REG_AC0DMAPTR)) {
        return false;
    }

    VNSvInPortD(dwIoBase + MAC_REG_RXDMAPTR0, &dwData);
    if (dwData != *(unsigned long *)(pbyCxtBuf + MAC_REG_RXDMAPTR0)) {
        return false;
    }

    VNSvInPortD(dwIoBase + MAC_REG_RXDMAPTR1, &dwData);
    if (dwData != *(unsigned long *)(pbyCxtBuf + MAC_REG_RXDMAPTR1)) {
        return false;
    }


    return true;
}

bool MACbSoftwareReset (unsigned long dwIoBase)
{
    unsigned char byData;
    unsigned short ww;

    
    
    VNSvOutPortB(dwIoBase+ MAC_REG_HOSTCR, 0x01);

    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortB(dwIoBase + MAC_REG_HOSTCR, &byData);
        if ( !(byData & HOSTCR_SOFTRST))
            break;
    }
    if (ww == W_MAX_TIMEOUT)
        return false;
    return true;

}

bool MACbSafeSoftwareReset (unsigned long dwIoBase)
{
    unsigned char abyTmpRegData[MAC_MAX_CONTEXT_SIZE_PAGE0+MAC_MAX_CONTEXT_SIZE_PAGE1];
    bool bRetVal;

    
    
    

    
    MACvSaveContext(dwIoBase, abyTmpRegData);
    
    bRetVal = MACbSoftwareReset(dwIoBase);
    
    
    MACvRestoreContext(dwIoBase, abyTmpRegData);

    return bRetVal;
}

bool MACbSafeRxOff (unsigned long dwIoBase)
{
    unsigned short ww;
    unsigned long dwData;
    unsigned char byData;

    

    
    VNSvOutPortD(dwIoBase + MAC_REG_RXDMACTL0, DMACTL_CLRRUN);
    VNSvOutPortD(dwIoBase + MAC_REG_RXDMACTL1, DMACTL_CLRRUN);
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortD(dwIoBase + MAC_REG_RXDMACTL0, &dwData);
        if (!(dwData & DMACTL_RUN))
            break;
    }
    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x10);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" DBG_PORT80(0x10)\n");
        return(false);
    }
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortD(dwIoBase + MAC_REG_RXDMACTL1, &dwData);
        if ( !(dwData & DMACTL_RUN))
            break;
    }
    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x11);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" DBG_PORT80(0x11)\n");
        return(false);
    }

    
    MACvRegBitsOff(dwIoBase, MAC_REG_HOSTCR, HOSTCR_RXON);
    
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortB(dwIoBase + MAC_REG_HOSTCR, &byData);
        if ( !(byData & HOSTCR_RXONST))
            break;
    }
    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x12);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" DBG_PORT80(0x12)\n");
        return(false);
    }
    return true;
}

bool MACbSafeTxOff (unsigned long dwIoBase)
{
    unsigned short ww;
    unsigned long dwData;
    unsigned char byData;

    
    
    VNSvOutPortD(dwIoBase + MAC_REG_TXDMACTL0, DMACTL_CLRRUN);
    
    VNSvOutPortD(dwIoBase + MAC_REG_AC0DMACTL, DMACTL_CLRRUN);


    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortD(dwIoBase + MAC_REG_TXDMACTL0, &dwData);
        if ( !(dwData & DMACTL_RUN))
            break;
    }
    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x20);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" DBG_PORT80(0x20)\n");
        return(false);
    }
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortD(dwIoBase + MAC_REG_AC0DMACTL, &dwData);
        if ( !(dwData & DMACTL_RUN))
            break;
    }
    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x21);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" DBG_PORT80(0x21)\n");
        return(false);
    }

    
    MACvRegBitsOff(dwIoBase, MAC_REG_HOSTCR, HOSTCR_TXON);

    
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortB(dwIoBase + MAC_REG_HOSTCR, &byData);
        if ( !(byData & HOSTCR_TXONST))
            break;
    }
    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x24);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" DBG_PORT80(0x24)\n");
        return(false);
    }
    return true;
}

bool MACbSafeStop (unsigned long dwIoBase)
{
    MACvRegBitsOff(dwIoBase, MAC_REG_TCR, TCR_AUTOBCNTX);

    if (MACbSafeRxOff(dwIoBase) == false) {
        DBG_PORT80(0xA1);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" MACbSafeRxOff == false)\n");
        MACbSafeSoftwareReset(dwIoBase);
        return false;
    }
    if (MACbSafeTxOff(dwIoBase) == false) {
        DBG_PORT80(0xA2);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" MACbSafeTxOff == false)\n");
        MACbSafeSoftwareReset(dwIoBase);
        return false;
    }

    MACvRegBitsOff(dwIoBase, MAC_REG_HOSTCR, HOSTCR_MACEN);

    return true;
}

bool MACbShutdown (unsigned long dwIoBase)
{
    
    MACvIntDisable(dwIoBase);
    MACvSetLoopbackMode(dwIoBase, MAC_LB_INTERNAL);
    
    if (!MACbSafeStop(dwIoBase)) {
        MACvSetLoopbackMode(dwIoBase, MAC_LB_NONE);
        return false;
    }
    MACvSetLoopbackMode(dwIoBase, MAC_LB_NONE);
    return true;
}

void MACvInitialize (unsigned long dwIoBase)
{
    
    MACvClearStckDS(dwIoBase);
    
    VNSvOutPortB(dwIoBase + MAC_REG_PMC1, PME_OVR);
    

    
    MACbSoftwareReset(dwIoBase);

    
    
    
    
    
    
    
    
    

    
    VNSvOutPortB(dwIoBase + MAC_REG_TFTCTL, TFTCTL_TSFCNTRST);
    
    VNSvOutPortB(dwIoBase + MAC_REG_TFTCTL, TFTCTL_TSFCNTREN);


    
    

    MACvSetPacketFilter(dwIoBase, PKT_TYPE_DIRECTED | PKT_TYPE_BROADCAST);

}

void MACvSetCurrRx0DescAddr (unsigned long dwIoBase, unsigned long dwCurrDescAddr)
{
unsigned short ww;
unsigned char byData;
unsigned char byOrgDMACtl;

    VNSvInPortB(dwIoBase + MAC_REG_RXDMACTL0, &byOrgDMACtl);
    if (byOrgDMACtl & DMACTL_RUN) {
        VNSvOutPortB(dwIoBase + MAC_REG_RXDMACTL0+2, DMACTL_RUN);
    }
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortB(dwIoBase + MAC_REG_RXDMACTL0, &byData);
        if ( !(byData & DMACTL_RUN))
            break;
    }
    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x13);
    }
    VNSvOutPortD(dwIoBase + MAC_REG_RXDMAPTR0, dwCurrDescAddr);
    if (byOrgDMACtl & DMACTL_RUN) {
        VNSvOutPortB(dwIoBase + MAC_REG_RXDMACTL0, DMACTL_RUN);
    }
}

void MACvSetCurrRx1DescAddr (unsigned long dwIoBase, unsigned long dwCurrDescAddr)
{
unsigned short ww;
unsigned char byData;
unsigned char byOrgDMACtl;

    VNSvInPortB(dwIoBase + MAC_REG_RXDMACTL1, &byOrgDMACtl);
    if (byOrgDMACtl & DMACTL_RUN) {
        VNSvOutPortB(dwIoBase + MAC_REG_RXDMACTL1+2, DMACTL_RUN);
    }
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortB(dwIoBase + MAC_REG_RXDMACTL1, &byData);
        if ( !(byData & DMACTL_RUN))
            break;
    }
    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x14);
    }
    VNSvOutPortD(dwIoBase + MAC_REG_RXDMAPTR1, dwCurrDescAddr);
    if (byOrgDMACtl & DMACTL_RUN) {
        VNSvOutPortB(dwIoBase + MAC_REG_RXDMACTL1, DMACTL_RUN);
    }
}

void MACvSetCurrTx0DescAddrEx (unsigned long dwIoBase, unsigned long dwCurrDescAddr)
{
unsigned short ww;
unsigned char byData;
unsigned char byOrgDMACtl;

    VNSvInPortB(dwIoBase + MAC_REG_TXDMACTL0, &byOrgDMACtl);
    if (byOrgDMACtl & DMACTL_RUN) {
        VNSvOutPortB(dwIoBase + MAC_REG_TXDMACTL0+2, DMACTL_RUN);
    }
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortB(dwIoBase + MAC_REG_TXDMACTL0, &byData);
        if ( !(byData & DMACTL_RUN))
            break;
    }
    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x25);
    }
    VNSvOutPortD(dwIoBase + MAC_REG_TXDMAPTR0, dwCurrDescAddr);
    if (byOrgDMACtl & DMACTL_RUN) {
        VNSvOutPortB(dwIoBase + MAC_REG_TXDMACTL0, DMACTL_RUN);
    }
}

 
void MACvSetCurrAC0DescAddrEx (unsigned long dwIoBase, unsigned long dwCurrDescAddr)
{
unsigned short ww;
unsigned char byData;
unsigned char byOrgDMACtl;

    VNSvInPortB(dwIoBase + MAC_REG_AC0DMACTL, &byOrgDMACtl);
    if (byOrgDMACtl & DMACTL_RUN) {
        VNSvOutPortB(dwIoBase + MAC_REG_AC0DMACTL+2, DMACTL_RUN);
    }
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortB(dwIoBase + MAC_REG_AC0DMACTL, &byData);
        if (!(byData & DMACTL_RUN))
            break;
    }
    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x26);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" DBG_PORT80(0x26)\n");
    }
    VNSvOutPortD(dwIoBase + MAC_REG_AC0DMAPTR, dwCurrDescAddr);
    if (byOrgDMACtl & DMACTL_RUN) {
        VNSvOutPortB(dwIoBase + MAC_REG_AC0DMACTL, DMACTL_RUN);
    }
}



void MACvSetCurrTXDescAddr (int iTxType, unsigned long dwIoBase, unsigned long dwCurrDescAddr)
{
    if(iTxType == TYPE_AC0DMA){
        MACvSetCurrAC0DescAddrEx(dwIoBase, dwCurrDescAddr);
    }else if(iTxType == TYPE_TXDMA0){
        MACvSetCurrTx0DescAddrEx(dwIoBase, dwCurrDescAddr);
    }
}

void MACvTimer0MicroSDelay (unsigned long dwIoBase, unsigned int uDelay)
{
unsigned char byValue;
unsigned int uu,ii;

    VNSvOutPortB(dwIoBase + MAC_REG_TMCTL0, 0);
    VNSvOutPortD(dwIoBase + MAC_REG_TMDATA0, uDelay);
    VNSvOutPortB(dwIoBase + MAC_REG_TMCTL0, (TMCTL_TMD | TMCTL_TE));
    for(ii=0;ii<66;ii++) {  
        for (uu = 0; uu < uDelay; uu++) {
            VNSvInPortB(dwIoBase + MAC_REG_TMCTL0, &byValue);
            if ((byValue == 0) ||
                (byValue & TMCTL_TSUSP)) {
                VNSvOutPortB(dwIoBase + MAC_REG_TMCTL0, 0);
                return;
            }
        }
    }
    VNSvOutPortB(dwIoBase + MAC_REG_TMCTL0, 0);

}

void MACvOneShotTimer0MicroSec (unsigned long dwIoBase, unsigned int uDelayTime)
{
    VNSvOutPortB(dwIoBase + MAC_REG_TMCTL0, 0);
    VNSvOutPortD(dwIoBase + MAC_REG_TMDATA0, uDelayTime);
    VNSvOutPortB(dwIoBase + MAC_REG_TMCTL0, (TMCTL_TMD | TMCTL_TE));
}

void MACvOneShotTimer1MicroSec (unsigned long dwIoBase, unsigned int uDelayTime)
{
    VNSvOutPortB(dwIoBase + MAC_REG_TMCTL1, 0);
    VNSvOutPortD(dwIoBase + MAC_REG_TMDATA1, uDelayTime);
    VNSvOutPortB(dwIoBase + MAC_REG_TMCTL1, (TMCTL_TMD | TMCTL_TE));
}


void MACvSetMISCFifo (unsigned long dwIoBase, unsigned short wOffset, unsigned long dwData)
{
    if (wOffset > 273)
        return;
    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFNDEX, wOffset);
    VNSvOutPortD(dwIoBase + MAC_REG_MISCFFDATA, dwData);
    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFCTL, MISCFFCTL_WRITE);
}


bool MACbTxDMAOff (unsigned long dwIoBase, unsigned int idx)
{
unsigned char byData;
unsigned int ww = 0;

    if (idx == TYPE_TXDMA0) {
        VNSvOutPortB(dwIoBase + MAC_REG_TXDMACTL0+2, DMACTL_RUN);
        for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
            VNSvInPortB(dwIoBase + MAC_REG_TXDMACTL0, &byData);
            if ( !(byData & DMACTL_RUN))
                break;
        }
    } else if (idx == TYPE_AC0DMA) {
        VNSvOutPortB(dwIoBase + MAC_REG_AC0DMACTL+2, DMACTL_RUN);
        for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
            VNSvInPortB(dwIoBase + MAC_REG_AC0DMACTL, &byData);
            if ( !(byData & DMACTL_RUN))
                break;
        }
    }
    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x29);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" DBG_PORT80(0x29)\n");
        return false;
    }
    return true;
}

void MACvClearBusSusInd (unsigned long dwIoBase)
{
    unsigned long dwOrgValue;
    unsigned int ww;
    
    VNSvInPortD(dwIoBase + MAC_REG_ENCFG , &dwOrgValue);
    if( !(dwOrgValue & EnCFG_BcnSusInd))
        return;
    
    dwOrgValue = dwOrgValue | EnCFG_BcnSusClr;
    VNSvOutPortD(dwIoBase + MAC_REG_ENCFG, dwOrgValue);
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortD(dwIoBase + MAC_REG_ENCFG , &dwOrgValue);
        if( !(dwOrgValue & EnCFG_BcnSusInd))
            break;
    }
    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x33);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" DBG_PORT80(0x33)\n");
    }
}

void MACvEnableBusSusEn (unsigned long dwIoBase)
{
    unsigned char byOrgValue;
    unsigned long dwOrgValue;
    unsigned int ww;
    
    VNSvInPortB(dwIoBase + MAC_REG_CFG , &byOrgValue);

    
    byOrgValue = byOrgValue | CFG_BCNSUSEN;
    VNSvOutPortB(dwIoBase + MAC_REG_ENCFG, byOrgValue);
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortD(dwIoBase + MAC_REG_ENCFG , &dwOrgValue);
        if(dwOrgValue & EnCFG_BcnSusInd)
            break;
    }
    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x34);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" DBG_PORT80(0x34)\n");
    }
}

bool MACbFlushSYNCFifo (unsigned long dwIoBase)
{
    unsigned char byOrgValue;
    unsigned int ww;
    
    VNSvInPortB(dwIoBase + MAC_REG_MACCR , &byOrgValue);

    
    byOrgValue = byOrgValue | MACCR_SYNCFLUSH;
    VNSvOutPortB(dwIoBase + MAC_REG_MACCR, byOrgValue);

    
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortB(dwIoBase + MAC_REG_MACCR , &byOrgValue);
        if(byOrgValue & MACCR_SYNCFLUSHOK)
            break;
    }
    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x35);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" DBG_PORT80(0x33)\n");
    }
    return true;
}

bool MACbPSWakeup (unsigned long dwIoBase)
{
    unsigned char byOrgValue;
    unsigned int ww;
    
    if (MACbIsRegBitsOff(dwIoBase, MAC_REG_PSCTL, PSCTL_PS)) {
        return true;
    }
    
    MACvRegBitsOff(dwIoBase, MAC_REG_PSCTL, PSCTL_PSEN);

    
    for (ww = 0; ww < W_MAX_TIMEOUT; ww++) {
        VNSvInPortB(dwIoBase + MAC_REG_PSCTL , &byOrgValue);
        if(byOrgValue & PSCTL_WAKEDONE)
            break;
    }
    if (ww == W_MAX_TIMEOUT) {
        DBG_PORT80(0x36);
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO" DBG_PORT80(0x33)\n");
        return false;
    }
    return true;
}


void MACvSetKeyEntry (unsigned long dwIoBase, unsigned short wKeyCtl, unsigned int uEntryIdx,
		unsigned int uKeyIdx, unsigned char *pbyAddr, unsigned long *pdwKey, unsigned char byLocalID)
{
unsigned short wOffset;
unsigned long dwData;
int     ii;

    if (byLocalID <= 1)
        return;


    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"MACvSetKeyEntry\n");
    wOffset = MISCFIFO_KEYETRY0;
    wOffset += (uEntryIdx * MISCFIFO_KEYENTRYSIZE);

    dwData = 0;
    dwData |= wKeyCtl;
    dwData <<= 16;
    dwData |= MAKEWORD(*(pbyAddr+4), *(pbyAddr+5));
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"1. wOffset: %d, Data: %lX, KeyCtl:%X\n", wOffset, dwData, wKeyCtl);

    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFNDEX, wOffset);
    VNSvOutPortD(dwIoBase + MAC_REG_MISCFFDATA, dwData);
    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFCTL, MISCFFCTL_WRITE);
    wOffset++;

    dwData = 0;
    dwData |= *(pbyAddr+3);
    dwData <<= 8;
    dwData |= *(pbyAddr+2);
    dwData <<= 8;
    dwData |= *(pbyAddr+1);
    dwData <<= 8;
    dwData |= *(pbyAddr+0);
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"2. wOffset: %d, Data: %lX\n", wOffset, dwData);

    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFNDEX, wOffset);
    VNSvOutPortD(dwIoBase + MAC_REG_MISCFFDATA, dwData);
    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFCTL, MISCFFCTL_WRITE);
    wOffset++;

    wOffset += (uKeyIdx * 4);
    for (ii=0;ii<4;ii++) {
        
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"3.(%d) wOffset: %d, Data: %lX\n", ii, wOffset+ii, *pdwKey);
        VNSvOutPortW(dwIoBase + MAC_REG_MISCFFNDEX, wOffset+ii);
        VNSvOutPortD(dwIoBase + MAC_REG_MISCFFDATA, *pdwKey++);
        VNSvOutPortW(dwIoBase + MAC_REG_MISCFFCTL, MISCFFCTL_WRITE);
    }
}



void MACvDisableKeyEntry (unsigned long dwIoBase, unsigned int uEntryIdx)
{
unsigned short wOffset;

    wOffset = MISCFIFO_KEYETRY0;
    wOffset += (uEntryIdx * MISCFIFO_KEYENTRYSIZE);

    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFNDEX, wOffset);
    VNSvOutPortD(dwIoBase + MAC_REG_MISCFFDATA, 0);
    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFCTL, MISCFFCTL_WRITE);
}



void MACvSetDefaultKeyEntry (unsigned long dwIoBase, unsigned int uKeyLen,
		unsigned int uKeyIdx, unsigned long *pdwKey, unsigned char byLocalID)
{
unsigned short wOffset;
unsigned long dwData;
int     ii;

    if (byLocalID <= 1)
        return;

    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"MACvSetDefaultKeyEntry\n");
    wOffset = MISCFIFO_KEYETRY0;
    wOffset += (10 * MISCFIFO_KEYENTRYSIZE);

    wOffset++;
    wOffset++;
    wOffset += (uKeyIdx * 4);
    
    for (ii=0; ii<3; ii++) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"(%d) wOffset: %d, Data: %lX\n", ii, wOffset+ii, *pdwKey);
        VNSvOutPortW(dwIoBase + MAC_REG_MISCFFNDEX, wOffset+ii);
        VNSvOutPortD(dwIoBase + MAC_REG_MISCFFDATA, *pdwKey++);
        VNSvOutPortW(dwIoBase + MAC_REG_MISCFFCTL, MISCFFCTL_WRITE);
    }
    dwData = *pdwKey;
    if (uKeyLen == WLAN_WEP104_KEYLEN) {
        dwData |= 0x80000000;
    }
    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFNDEX, wOffset+3);
    VNSvOutPortD(dwIoBase + MAC_REG_MISCFFDATA, dwData);
    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFCTL, MISCFFCTL_WRITE);
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"End. wOffset: %d, Data: %lX\n", wOffset+3, dwData);

}



void MACvDisableDefaultKey (unsigned long dwIoBase)
{
unsigned short wOffset;
unsigned long dwData;


    wOffset = MISCFIFO_KEYETRY0;
    wOffset += (10 * MISCFIFO_KEYENTRYSIZE);

    dwData = 0x0;
    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFNDEX, wOffset);
    VNSvOutPortD(dwIoBase + MAC_REG_MISCFFDATA, dwData);
    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFCTL, MISCFFCTL_WRITE);
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"MACvDisableDefaultKey: wOffset: %d, Data: %lX\n", wOffset, dwData);
}

void MACvSetDefaultTKIPKeyEntry (unsigned long dwIoBase, unsigned int uKeyLen,
		unsigned int uKeyIdx, unsigned long *pdwKey, unsigned char byLocalID)
{
unsigned short wOffset;
unsigned long dwData;
int     ii;

    if (byLocalID <= 1)
        return;


    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"MACvSetDefaultTKIPKeyEntry\n");
    wOffset = MISCFIFO_KEYETRY0;
    
    wOffset += (10 * MISCFIFO_KEYENTRYSIZE);

    dwData = 0xC0660000;
    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFNDEX, wOffset);
    VNSvOutPortD(dwIoBase + MAC_REG_MISCFFDATA, dwData);
    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFCTL, MISCFFCTL_WRITE);
    wOffset++;

    dwData = 0;
    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFNDEX, wOffset);
    VNSvOutPortD(dwIoBase + MAC_REG_MISCFFDATA, dwData);
    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFCTL, MISCFFCTL_WRITE);
    wOffset++;

    wOffset += (uKeyIdx * 4);
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"1. wOffset: %d, Data: %lX, idx:%d\n", wOffset, *pdwKey, uKeyIdx);
    
    for (ii=0; ii<4; ii++) {
        DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"2.(%d) wOffset: %d, Data: %lX\n", ii, wOffset+ii, *pdwKey);
        VNSvOutPortW(dwIoBase + MAC_REG_MISCFFNDEX, wOffset+ii);
        VNSvOutPortD(dwIoBase + MAC_REG_MISCFFDATA, *pdwKey++);
        VNSvOutPortW(dwIoBase + MAC_REG_MISCFFCTL, MISCFFCTL_WRITE);
    }

}




void MACvSetDefaultKeyCtl (unsigned long dwIoBase, unsigned short wKeyCtl, unsigned int uEntryIdx, unsigned char byLocalID)
{
unsigned short wOffset;
unsigned long dwData;

    if (byLocalID <= 1)
        return;


    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"MACvSetKeyEntry\n");
    wOffset = MISCFIFO_KEYETRY0;
    wOffset += (uEntryIdx * MISCFIFO_KEYENTRYSIZE);

    dwData = 0;
    dwData |= wKeyCtl;
    dwData <<= 16;
    dwData |= 0xffff;
    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"1. wOffset: %d, Data: %lX, KeyCtl:%X\n", wOffset, dwData, wKeyCtl);

    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFNDEX, wOffset);
    VNSvOutPortD(dwIoBase + MAC_REG_MISCFFDATA, dwData);
    VNSvOutPortW(dwIoBase + MAC_REG_MISCFFCTL, MISCFFCTL_WRITE);

}

