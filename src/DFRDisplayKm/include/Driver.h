/*++

Module Name:

    driver.h

Abstract:

    This file contains the driver definitions.

Environment:

    Kernel-mode Driver Framework

--*/

#include <ntddk.h>
#include <wdf.h>
#include <usb.h>
#include <usbdlib.h>
#include <wdfusb.h>
#include <initguid.h>

#include "device.h"
#include "queue.h"
#include "trace.h"

#include <Dfr.h>
#include <DFRHostIo.h>

EXTERN_C_START

//
// WDFDRIVER Events
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD DFRDisplayKmEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP DFRDisplayKmEvtDriverContextCleanup;

EXTERN_C_END
