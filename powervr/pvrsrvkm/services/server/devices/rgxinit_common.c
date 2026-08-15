/*************************************************************************/ /*!
@File           rgxinit_common.c
@Title          RGX device specific internal/external initialisation routines
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    RGX device specific functions
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include "rgxinit.h"
#include "rgxinit_internal.h"

#include "allocmem.h"
#include "rgx_heaps_server.h"

#include "rgxmem.h"
#include "rgxbvnc.h"

#include "rgxhwperf_common.h"
#include "rgxdebug_common.h"

#include "rgxfwutils.h"
#include "rgx_fwif_alignchecks.h"

#include "pvrsrv_apphint.h"
#include "os_apphint.h"

static PVRSRV_ERROR RGXAllocUFOBlock(PVRSRV_DEVICE_NODE *psDeviceNode,
                                     IMG_UINT32 ui32RequestedSize,
                                     DEVMEM_MEMDESC **psMemDesc,
                                     IMG_UINT32 *puiSyncPrimVAddr,
                                     IMG_UINT32 *puiSyncPrimBlockSize)
{
	PVRSRV_RGXDEV_INFO *psDevInfo;
	PVRSRV_ERROR eError;
	RGXFWIF_DEV_VIRTADDR pFirmwareAddr;
	IMG_DEVMEM_ALIGN_T uiUFOBlockAlign = MAX(sizeof(IMG_UINT32), sizeof(SYNC_CHECKPOINT_FW_OBJ));
	IMG_DEVMEM_SIZE_T uiUFOBlockSize = PVR_ALIGN(ui32RequestedSize, uiUFOBlockAlign);

	psDevInfo = psDeviceNode->pvDevice;

	/* Size and align are 'expanded' because we request an Exportalign allocation */
	eError = DevmemExportalignAdjustSizeAndAlign(DevmemGetHeapLog2PageSize(psDevInfo->psFirmwareMainHeap),
	                                             &uiUFOBlockSize,
	                                             &uiUFOBlockAlign);

	if (eError != PVRSRV_OK)
	{
		goto e0;
	}

	eError = DevmemFwAllocateExportable(psDeviceNode,
	                                    uiUFOBlockSize,
	                                    uiUFOBlockAlign,
	                                    PVRSRV_MEMALLOCFLAG_PHYS_HEAP_HINT(FW_MAIN) |
	                                    PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT) |
	                                    PVRSRV_MEMALLOCFLAG_KERNEL_CPU_MAPPABLE |
	                                    PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC |
	                                    PVRSRV_MEMALLOCFLAG_GPU_READABLE |
	                                    PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE |
	                                    PVRSRV_MEMALLOCFLAG_CPU_READABLE |
	                                    PVRSRV_MEMALLOCFLAG_CPU_WRITEABLE |
	                                    PVRSRV_MEMALLOCFLAG_UNCACHED,
	                                    "FwExUFOBlock",
	                                    psMemDesc);
	if (eError != PVRSRV_OK)
	{
		goto e0;
	}

	eError = RGXSetFirmwareAddress(&pFirmwareAddr, *psMemDesc, 0, RFW_FWADDR_FLAG_NONE);
	PVR_GOTO_IF_ERROR(eError, e1);

	*puiSyncPrimVAddr = pFirmwareAddr.ui32Addr;
	*puiSyncPrimBlockSize = TRUNCATE_64BITS_TO_32BITS(uiUFOBlockSize);

	return PVRSRV_OK;

e1:
	DevmemFwUnmapAndFree(psDevInfo, *psMemDesc);
e0:
	return eError;
}

static void RGXFreeUFOBlock(PVRSRV_DEVICE_NODE *psDeviceNode,
                            DEVMEM_MEMDESC *psMemDesc)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

	RGXUnsetFirmwareAddress(psMemDesc);
	DevmemFwUnmapAndFree(psDevInfo, psMemDesc);
}

/**************************************************************************/ /*!
@Function       RGXDevClockSpeed
@Description    Gets the clock speed for the given device node and returns
                it in pui32RGXClockSpeed.
@Input          psDeviceNode        Device node
@Output         pui32RGXClockSpeed  Variable for storing the clock speed
@Return         PVRSRV_ERROR
*/ /**************************************************************************/
static PVRSRV_ERROR RGXDevClockSpeed(PVRSRV_DEVICE_NODE *psDeviceNode,
                                     IMG_PUINT32  pui32RGXClockSpeed)
{
	RGX_DATA *psRGXData = (RGX_DATA*) psDeviceNode->psDevConfig->hDevData;

	/* get clock speed */
	*pui32RGXClockSpeed = psRGXData->psRGXTimingInfo->ui32CoreClockSpeed;

	return PVRSRV_OK;
}

/*************************************************************************/ /*!
@Function       RGXDevVersionString
@Description    Gets the version string for the given device node and returns
                a pointer to it in ppszVersionString. It is then the
                responsibility of the caller to free this memory.
@Input          psDeviceNode        Device node from which to obtain the
                                    version string
@Output.        ppszVersionString   Contains the version string upon return
@Return         PVRSRV_ERROR
*/ /**************************************************************************/
static PVRSRV_ERROR RGXDevVersionString(PVRSRV_DEVICE_NODE *psDeviceNode,
                                        IMG_CHAR **ppszVersionString)
{
#if defined(NO_HARDWARE)
	const IMG_CHAR *pszFormatString = "GPU variant BVNC: %s (SW)";
#else
	const IMG_CHAR *pszFormatString = PVRSRVIsEmulatorPlatform(psDeviceNode) ?
		"GPU variant BVNC: %s (SW)" : "GPU variant BVNC: %s (HW)";
#endif
	PVRSRV_RGXDEV_INFO *psDevInfo;
	IMG_PCHAR pszBVNC;
	size_t uiStringLength;

	if (psDeviceNode == NULL || ppszVersionString == NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDevInfo = (PVRSRV_RGXDEV_INFO *)psDeviceNode->pvDevice;
	pszBVNC = RGXDevBVNCString(psDevInfo);

	if (NULL == pszBVNC)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	/* -2 for %s. +1 for NULL terminate */
	uiStringLength = OSStringLength(pszBVNC) + (OSStringLength(pszFormatString) - 2) + 1;
	*ppszVersionString = OSAllocMem(uiStringLength * sizeof(IMG_CHAR));
	if (*ppszVersionString == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	OSSNPrintf(*ppszVersionString, uiStringLength, pszFormatString, pszBVNC);

	return PVRSRV_OK;
}

/*
	RGXDevMMUAttributes
*/
static MMU_DEVICEATTRIBS *RGXDevMMUAttributes(PVRSRV_DEVICE_NODE *psDeviceNode,
                                              IMG_BOOL bKernelFWMemoryCtx)
{
	MMU_DEVICEATTRIBS *psMMUDevAttrs = NULL;

	if (psDeviceNode->pfnCheckDeviceFeature)
	{
#if defined(RGX_FEATURE_MIPS_BIT_MASK)
		if (PVRSRV_IS_FEATURE_SUPPORTED(psDeviceNode, MIPS))
		{
			psMMUDevAttrs = bKernelFWMemoryCtx ?
			                psDeviceNode->psFirmwareMMUDevAttrs :
			                psDeviceNode->psMMUDevAttrs;
		}
		else
#endif
		{
			PVR_UNREFERENCED_PARAMETER(bKernelFWMemoryCtx);
			psMMUDevAttrs = psDeviceNode->psMMUDevAttrs;
		}
	}

	return psMMUDevAttrs;
}

static PVRSRV_ERROR RGXAlignmentCheck(PVRSRV_DEVICE_NODE *psDevNode,
                                      IMG_UINT32 ui32AlignChecksSizeUM,
                                      IMG_UINT32 aui32AlignChecksUM[])
{
	static const IMG_UINT32 aui32AlignChecksKM[] = {RGXFW_ALIGN_CHECKS_INIT_KM};
	IMG_UINT32 ui32UMChecksOffset = ARRAY_SIZE(aui32AlignChecksKM) + 1;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDevNode->pvDevice;
	IMG_UINT32 i, *paui32FWAlignChecks;
	PVRSRV_ERROR eError = PVRSRV_OK;

	/* Skip the alignment check if the driver is guest
	   since there is no firmware to check against */
	PVRSRV_VZ_RET_IF_MODE(GUEST, DEVNODE, psDevNode, eError);

	if (psDevInfo->psRGXFWAlignChecksMemDesc == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: FW Alignment Check Mem Descriptor is NULL",
		         __func__));
		return PVRSRV_ERROR_ALIGNMENT_ARRAY_NOT_AVAILABLE;
	}

	eError = DevmemAcquireCpuVirtAddr(psDevInfo->psRGXFWAlignChecksMemDesc,
	                                  (void **) &paui32FWAlignChecks);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Failed to acquire kernel address for alignment checks (%u)",
		         __func__,
		         eError));
		return eError;
	}

	paui32FWAlignChecks += ui32UMChecksOffset;
	/* Invalidate the size value, check the next region size (UM) and invalidate */
	RGXFwSharedMemCacheOpPtr(paui32FWAlignChecks, INVALIDATE);
	if (*paui32FWAlignChecks++ != ui32AlignChecksSizeUM)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Mismatching sizes of RGXFW_ALIGN_CHECKS_INIT"
		         " array between UM(%d) and FW(%d)",
		         __func__,
		         ui32AlignChecksSizeUM,
		         *paui32FWAlignChecks));
		eError = PVRSRV_ERROR_INVALID_ALIGNMENT;
		goto return_;
	}

	RGXFwSharedMemCacheOpExec(paui32FWAlignChecks,
	                          ui32AlignChecksSizeUM * sizeof(IMG_UINT32),
	                          PVRSRV_CACHE_OP_INVALIDATE);

	for (i = 0; i < ui32AlignChecksSizeUM; i++)
	{
		if (aui32AlignChecksUM[i] != paui32FWAlignChecks[i])
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: size/offset mismatch in RGXFW_ALIGN_CHECKS_INIT[%d]"
					" between UM(%d) and FW(%d)",
					__func__, i, aui32AlignChecksUM[i], paui32FWAlignChecks[i]));
			eError = PVRSRV_ERROR_INVALID_ALIGNMENT;
		}
	}

	if (eError == PVRSRV_ERROR_INVALID_ALIGNMENT)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Check for FW/KM structure"
				" alignment failed.", __func__));
	}

return_:

	DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWAlignChecksMemDesc);

	return eError;
}

/*
	RGXGetTFBCLossyGroup
*/
static IMG_UINT32 RGXGetTFBCLossyGroup(PVRSRV_DEVICE_NODE *psDeviceNode)
{
#if defined(RGX_FEATURE_TFBC_LOSSY_37_PERCENT_BIT_MASK)
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	return psDevInfo->ui32TFBCLossyGroup;
#else
	PVR_UNREFERENCED_PARAMETER(psDeviceNode);
	return 0;
#endif
}

void RGXDeviceInitCallbacks(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVR_ASSERT(psDeviceNode != NULL);

	/* Callback for getting the MMU device attributes */
	psDeviceNode->pfnGetMMUDeviceAttributes = RGXDevMMUAttributes;
	psDeviceNode->pfnMMUCacheInvalidate = RGXMMUCacheInvalidate;
	psDeviceNode->pfnMMUCacheInvalidateKick = RGXMMUCacheInvalidateKick;

#if defined(RGX_BRN71422_TARGET_HARDWARE_PHYSICAL_ADDR)
	psDeviceNode->pfnMMUTopLevelPxWorkarounds = RGXMapBRN71422TargetPhysicalAddress;
#else
	psDeviceNode->pfnMMUTopLevelPxWorkarounds = NULL;
#endif

	psDeviceNode->pfnRegisterMemoryContext = RGXRegisterMemoryContext;
	psDeviceNode->pfnUnregisterMemoryContext = RGXUnregisterMemoryContext;

	psDeviceNode->pfnValidateAddressPermissions = RGXValidateAddressPermissions;
	psDeviceNode->pfnValidateExportableFlags = RGXValidateExportableFlags;

	/* Register callbacks for Unified Fence Objects */
	psDeviceNode->pfnAllocUFOBlock = RGXAllocUFOBlock;
	psDeviceNode->pfnFreeUFOBlock = RGXFreeUFOBlock;

#if defined(SUPPORT_AUTOVZ)
	/* Register callback for updating the virtualization watchdog */
	psDeviceNode->pfnUpdateAutoVzWatchdog = RGXUpdateAutoVzWatchdog;
#endif

	/* Register method to service the FW HWPerf buffer */
	psDeviceNode->pfnServiceHWPerf = RGXHWPerfDataStoreCB;

	/* Register callback for getting the device version information string */
	psDeviceNode->pfnDeviceVersionString = RGXDevVersionString;

	/* Register callback for getting the device clock speed */
	psDeviceNode->pfnDeviceClockSpeed = RGXDevClockSpeed;

	/* Register callback for resetting the HWR logs */
	psDeviceNode->pfnResetHWRLogs = RGXResetHWRLogs;

	/* Register callback for resetting the HWR logs */
	psDeviceNode->pfnVerifyBVNC = RGXVerifyBVNC;

	/* Register callback for checking alignment of UM structures */
	psDeviceNode->pfnAlignmentCheck = RGXAlignmentCheck;

	/* Register callback for checking the supported features and getting the
	 * corresponding values */
	psDeviceNode->pfnCheckDeviceFeature = RGXBvncCheckFeatureSupported;
	psDeviceNode->pfnGetDeviceFeatureValue = RGXBvncGetSupportedFeatureValue;

	/* Callback for getting TFBC configuration */
	psDeviceNode->pfnGetTFBCLossyGroup = RGXGetTFBCLossyGroup;
}

IMG_PCHAR RGXDevBVNCString(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	IMG_PCHAR psz = psDevInfo->sDevFeatureCfg.pszBVNCString;
	if (NULL == psz)
	{
		IMG_CHAR pszBVNCInfo[RGX_HWPERF_MAX_BVNC_LEN];
		size_t uiBVNCStringSize;
		size_t uiStringLength;

		uiStringLength = OSSNPrintf(pszBVNCInfo,
		                            RGX_HWPERF_MAX_BVNC_LEN,
		                            "%d.%d.%d.%d",
		                            psDevInfo->sDevFeatureCfg.ui32B,
		                            psDevInfo->sDevFeatureCfg.ui32V,
		                            psDevInfo->sDevFeatureCfg.ui32N,
		                            psDevInfo->sDevFeatureCfg.ui32C);
		PVR_ASSERT(uiStringLength < RGX_HWPERF_MAX_BVNC_LEN);

		uiBVNCStringSize = (uiStringLength + 1) * sizeof(IMG_CHAR);
		psz = OSAllocMem(uiBVNCStringSize);
		if (NULL != psz)
		{
			OSCachedMemCopy(psz, pszBVNCInfo, uiBVNCStringSize);
			psDevInfo->sDevFeatureCfg.pszBVNCString = psz;
		}
		else
		{
			PVR_DPF((PVR_DBG_MESSAGE,
			         "%s: Allocating memory for BVNC Info string failed",
			         __func__));
		}
	}

	return psz;
}

IMG_UINT32 RGXHeapDerivePageSize(IMG_UINT32 uiLog2PageSize)
{
	IMG_BOOL bFound = IMG_FALSE;
	IMG_UINT32 ui32PageSizeMask = RGXGetValidHeapPageSizeMask();

	/* OS page shift must be at least RGX_HEAP_4KB_PAGE_SHIFT,
	 * max RGX_HEAP_2MB_PAGE_SHIFT, non-zero and a power of two */
	if (uiLog2PageSize == 0U ||
	    (uiLog2PageSize < RGX_HEAP_4KB_PAGE_SHIFT) ||
	    (uiLog2PageSize > RGX_HEAP_2MB_PAGE_SHIFT))
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Provided incompatible log2 page size %u",
				__func__,
				uiLog2PageSize));
		PVR_ASSERT(0);
		return 0;
	}

	do
	{
		if ((IMG_PAGE2BYTES32(uiLog2PageSize) & ui32PageSizeMask) == 0)
		{
			/* We have to fall back to a smaller device
			 * page size than given page size because there
			 * is no exact match for any supported size. */
			uiLog2PageSize -= 1U;
		}
		else
		{
			/* All good, RGX page size equals given page size
			 * => use it as default for heaps */
			bFound = IMG_TRUE;
		}
	} while (!bFound);

	return uiLog2PageSize;
}

#if !defined(NO_HARDWARE)
IMG_BOOL SampleIRQCount(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	IMG_BOOL bReturnVal = IMG_FALSE;
	volatile IMG_UINT32 *pui32SampleIrqCount = psDevInfo->aui32SampleIRQCount;
	IMG_UINT32 ui32IrqCnt;

#if defined(RGX_FW_IRQ_OS_COUNTERS)
	if (PVRSRV_VZ_MODE_IS(GUEST, DEVINFO, psDevInfo))
	{
		bReturnVal = IMG_TRUE;
	}
	else
	{
		get_irq_cnt_val(ui32IrqCnt, RGXFW_HOST_DRIVER_ID, psDevInfo);

		if (ui32IrqCnt != pui32SampleIrqCount[RGXFW_THREAD_0])
		{
			pui32SampleIrqCount[RGXFW_THREAD_0] = ui32IrqCnt;
			bReturnVal = IMG_TRUE;
		}
	}
#else
	IMG_UINT32 ui32TID;

	for_each_irq_cnt(ui32TID)
	{
		get_irq_cnt_val(ui32IrqCnt, ui32TID, psDevInfo);

		/* treat unhandled interrupts here to align host count with fw count */
		if (pui32SampleIrqCount[ui32TID] != ui32IrqCnt)
		{
			pui32SampleIrqCount[ui32TID] = ui32IrqCnt;
			bReturnVal = IMG_TRUE;
		}
	}
#endif

	return bReturnVal;
}

static IMG_BOOL _WaitForInterruptsTimeoutCheck(PVRSRV_RGXDEV_INFO *psDevInfo)
{
#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
	PVRSRV_DEVICE_NODE *psDeviceNode = psDevInfo->psDeviceNode;
	IMG_UINT32 ui32idx;
#endif

	RGXDEBUG_PRINT_IRQ_COUNT(psDevInfo);

#if defined(PVRSRV_DEBUG_LISR_EXECUTION)
	PVR_DPF((PVR_DBG_ERROR,
	        "Last RGX_LISRHandler State (DevID %u): 0x%08X Clock: %" IMG_UINT64_FMTSPEC,
	        psDeviceNode->sDevId.ui32InternalID,
	        psDeviceNode->sLISRExecutionInfo.ui32Status,
	        psDeviceNode->sLISRExecutionInfo.ui64Clockns));

	for_each_irq_cnt(ui32idx)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         MSG_IRQ_CNT_TYPE " %u: InterruptCountSnapshot: 0x%X",
		         ui32idx, psDeviceNode->sLISRExecutionInfo.aui32InterruptCountSnapshot[ui32idx]));
	}
#else
	PVR_DPF((PVR_DBG_ERROR, "No further information available. Please enable PVRSRV_DEBUG_LISR_EXECUTION"));
#endif

	return SampleIRQCount(psDevInfo);
}

void RGX_WaitForInterruptsTimeout(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	IMG_BOOL bScheduleMISR;

	if (PVRSRV_VZ_MODE_IS(GUEST, DEVINFO, psDevInfo))
	{
		bScheduleMISR = IMG_TRUE;
	}
	else
	{
		bScheduleMISR = _WaitForInterruptsTimeoutCheck(psDevInfo);
	}

	if (bScheduleMISR)
	{
		OSScheduleMISR(psDevInfo->pvMISRData);

		if (psDevInfo->pvAPMISRData != NULL)
		{
			OSScheduleMISR(psDevInfo->pvAPMISRData);
		}
	}
}

IMG_BOOL RGXAckHwIrq(PVRSRV_RGXDEV_INFO *psDevInfo,
                     IMG_UINT32 ui32IRQStatusReg,
                     IMG_UINT32 ui32IRQStatusEventMsk,
                     IMG_UINT32 ui32IRQClearReg,
                     IMG_UINT32 ui32IRQClearMask)
{
	IMG_UINT32 ui32IRQStatus = OSReadHWReg32(psDevInfo->pvRegsBaseKM, ui32IRQStatusReg);

	/* clear only the pending bit of the thread that triggered this interrupt */
	ui32IRQClearMask &= ui32IRQStatus;

	if (ui32IRQStatus & ui32IRQStatusEventMsk)
	{
		/* acknowledge and clear the interrupt */
		OSWriteHWReg32(psDevInfo->pvRegsBaseKM, ui32IRQClearReg, ui32IRQClearMask);
		/* Perform a readback as barrier here after clearing the interrupt.
		 * If host side mem read happens before we clear the interrupt it is possible
		 * that we read the stale value then fw updates the second interrupt which is
		 * ignored and then we clear interrupt which would mean host will end up with
		 * a stale IRQCount value.
		 */
		(void) OSReadHWReg32(psDevInfo->pvRegsBaseKM, ui32IRQClearReg);

		return IMG_TRUE;
	}
	else
	{
		/* spurious interrupt */
		return IMG_FALSE;
	}
}

IMG_BOOL RGXAckIrqDedicated(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	/* status & clearing registers are available on both Host and Guests
	 * and are agnostic of the Fw CPU type. Due to the remappings done by
	 * the 2nd stage device MMU, all drivers assume they are accessing
	 * register bank 0 */
	return RGXAckHwIrq(psDevInfo,
	                   RGX_CR_IRQ_OS0_EVENT_STATUS,
	                   ~RGX_CR_IRQ_OS0_EVENT_STATUS_SOURCE_CLRMSK,
	                   RGX_CR_IRQ_OS0_EVENT_CLEAR,
	                   ~RGX_CR_IRQ_OS0_EVENT_CLEAR_SOURCE_CLRMSK);
}
#endif

#if !defined(SUPPORT_TRUSTED_DEVICE)
#if defined(RGX_FEATURE_AXI_ACE_BIT_MASK) || defined(RGX_FEATURE_AXI_ACELITE_BIT_MASK)

void DoRGXInitialiseCacheSnooping(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_DEVICE_CONFIG *psDevConfig;

	psDevConfig = psDeviceNode->psDevConfig;

	/* Check App Hint (only available on volcanic cores) */
#if defined(PVRSRV_APPHINT_FABRICCOHERENCYOVERRIDE)
#if PVRSRV_APPHINT_FABRICCOHERENCYOVERRIDE == 0
	/* Cache Snooping mode can only be downgraded by the App Hint */
	psDeviceNode->eConfirmedCacheSnoopingMode = PVRSRV_DEVICE_SNOOP_NONE;
	PVR_DPF((PVR_DBG_WARNING,"%s Cache coherency downgraded by app hint", __func__));
	return;
#endif
	{
		IMG_UINT32 ui32AppHintFabricCoherency;
		IMG_UINT32 ui32AppHintDefault = 1;
		void *pvAppHintState = NULL;

		OSCreateAppHintState(&pvAppHintState);
		OSGetAppHintUINT32(APPHINT_NO_DEVICE, pvAppHintState, FabricCoherencyOverride,
		                   &ui32AppHintDefault, &ui32AppHintFabricCoherency);
		OSFreeAppHintState(pvAppHintState);
		if (ui32AppHintFabricCoherency == 0)
		{
			/* Cache Snooping mode can only be downgraded by the App Hint */
			psDeviceNode->eConfirmedCacheSnoopingMode = PVRSRV_DEVICE_SNOOP_NONE;
			PVR_DPF((PVR_DBG_WARNING,"%s Cache coherency downgraded by kernel app hint", __func__));
			return;
		}
	}
#endif
	/* if the device supports coherency */
	if (psDevConfig->eCacheSnoopingMode == PVRSRV_DEVICE_SNOOP_CPU_ONLY)
	{
		/* and the device is configured to support coherency (see RGXQueryCoreCoherencyFeature) */
		if (psDeviceNode->eGpuSnoopingFeature == PVRSRV_DEVICE_SNOOP_CPU_ONLY)
		{
			/* and the fabric (e.g. PCIe or AXI bus) supports coherency */
			if (psDeviceNode->eDevFabricType == PVRSRV_DEVICE_FABRIC_ACELITE)
			{
				/* Provisionally initialise cache snooping, as it may be
				 * downgraded a bit later when we run coherency tests */
				psDeviceNode->eConfirmedCacheSnoopingMode = psDevConfig->eCacheSnoopingMode;
				return;
			}
		}
	}
	psDeviceNode->eConfirmedCacheSnoopingMode = PVRSRV_DEVICE_SNOOP_NONE;
}

void RGXInitialiseCacheSnooping(const void *hPrivate)
{
	PVRSRV_RGXDEV_INFO *psDevInfo;
	PVRSRV_DEVICE_NODE *psDeviceNode;

	PVR_ASSERT(hPrivate != NULL);

	psDevInfo = ((RGX_LAYER_PARAMS*) hPrivate)->psDevInfo;
	psDeviceNode = psDevInfo->psDeviceNode;
	DoRGXInitialiseCacheSnooping(psDeviceNode);
}


void RGXDisableCacheSnooping(const void *hPrivate)
{
	PVRSRV_RGXDEV_INFO *psDevInfo;
	PVRSRV_DEVICE_NODE *psDeviceNode;

	PVR_ASSERT(hPrivate != NULL);

	psDevInfo = ((RGX_LAYER_PARAMS*) hPrivate)->psDevInfo;
	psDeviceNode = psDevInfo->psDeviceNode;

	/* disable the confirmed coherency level */
	psDeviceNode->eConfirmedCacheSnoopingMode = PVRSRV_DEVICE_SNOOP_NONE;
}
#endif /* RGX_FEATURE_AXI_ACE_BIT_MASK || RGX_FEATURE_AXI_ACELITE_BIT_MASK */
#endif /* !SUPPORT_TRUSTED_DEVICE */