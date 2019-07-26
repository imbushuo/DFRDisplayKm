#include "public.h"

EXTERN_C_START

//
// The device context performs the same job as
// a WDM device extension in the driver frameworks
//
typedef struct _DEVICE_CONTEXT
{
	// WDF USB
    WDFUSBDEVICE UsbDevice;
	WDFUSBINTERFACE UsbInterface;
	WDFUSBPIPE BulkPipeIn;
	WDFUSBPIPE BulkPipeOut;

	// Device information
	UINT16 Width;
	UINT16 Height;
	UINT32 FrameBufferPixelFormat;
	BOOLEAN DeviceReady;
	UINT32 CurrentFrameId;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

//
// Our pool tag (T,B,A,R)
//
#define POOL_TAG 0x54424152

//
// This macro will generate an inline function called DeviceGetContext
// which will be used to get a pointer to the device context memory
// in a type safe manner.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

//
// Function to initialize the device's queues and callbacks
//
NTSTATUS
DFRDisplayKmCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    );

//
// Function to select the device's USB configuration and get a WDFUSBDEVICE
// handle
//
EVT_WDF_DEVICE_PREPARE_HARDWARE DFRDisplayKmEvtDevicePrepareHardware;
EVT_WDF_DEVICE_D0_ENTRY DFRDisplayEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT DFRDisplayEvtDeviceD0Exit;

//
// IOCTL handler
//
NTSTATUS
DFRDisplayUpdateFrameBuffer(
	_In_ WDFREQUEST Request,
	_In_ WDFDEVICE Device,
	_In_ size_t InputBufferLength,
	_Out_ BOOLEAN *RequestPending
);

//
// Functions for USB transfer
//
NTSTATUS
DFRDisplaySendBufferSynchronously(
	_In_ PDEVICE_CONTEXT pDeviceContext,
	_In_ PVOID Buffer,
	_In_ ULONG BufferSize
);

NTSTATUS
DFRDisplayGetBufferSynchronously(
	_In_ PDEVICE_CONTEXT pDeviceContext,
	_In_ PVOID Buffer,
	_In_ ULONG BufferSize,
	_Out_ PULONG PipeBytesTransferrred
);

NTSTATUS
DFRDisplaySendGenericRequestAndForget(
	_In_ PDEVICE_CONTEXT pDeviceContext,
	_In_ UINT32 RequestKey
);

NTSTATUS
DFRDisplayTransferFrameBuffer(
	_In_ WDFDEVICE Device,
	_In_ UINT16 BeginPosX,
	_In_ UINT16 BeginPosY,
	_In_ UINT16 Width,
	_In_ UINT16 Height,
	_In_ PVOID FrameBuffer,
	_In_ size_t FrameBufferLength
);

//
// Functions to support debug & diagnostics
//
PCHAR
DbgDevicePowerString(
	_In_ WDF_POWER_DEVICE_STATE Type
);

EXTERN_C_END
