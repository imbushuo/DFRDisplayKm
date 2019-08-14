#include <Driver.h>
#include "device.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, DFRDisplayKmCreateDevice)
#pragma alloc_text (PAGE, DFRDisplayKmEvtDevicePrepareHardware)
#endif


NTSTATUS
DFRDisplayKmCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;
    WDF_OBJECT_ATTRIBUTES   deviceAttributes;
    PDEVICE_CONTEXT deviceContext;
    WDFDEVICE device;
    NTSTATUS status;
	LARGE_INTEGER PerfCounter;

    PAGED_CODE();

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
    pnpPowerCallbacks.EvtDevicePrepareHardware = DFRDisplayKmEvtDevicePrepareHardware;
	pnpPowerCallbacks.EvtDeviceD0Entry = DFRDisplayEvtDeviceD0Entry;
	pnpPowerCallbacks.EvtDeviceD0Exit = DFRDisplayEvtDeviceD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

	WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_DFR);
    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

    if (NT_SUCCESS(status)) {
        //
        // Get a pointer to the device context structure that we just associated
        // with the device object. We define this structure in the device.h
        // header file. DeviceGetContext is an inline function generated by
        // using the WDF_DECLARE_CONTEXT_TYPE_WITH_NAME macro in device.h.
        // This function will do the type checking and return the device context.
        // If you pass a wrong object handle it will return NULL and assert if
        // run under framework verifier mode.
        //
        deviceContext = DeviceGetContext(device);

		KeQueryPerformanceCounter(&PerfCounter);
		deviceContext->CurrentFrameId = (((PerfCounter.QuadPart >> 32) >> 16) >> 8);
		deviceContext->DeviceReady = FALSE;
		deviceContext->DeviceScreenCleared = FALSE;

        //
        // Create a device interface so that applications can find and talk
        // to us.
        //
        status = WdfDeviceCreateDeviceInterface(
            device,
            &GUID_DEVINTERFACE_DFRDisplayKm,
            NULL // ReferenceString
            );

        if (NT_SUCCESS(status)) {
            //
            // Initialize the I/O Package and any Queues
            //
            status = DFRDisplayKmQueueInitialize(device);
        }
    }

    return status;
}

NTSTATUS
DFRDisplayKmEvtDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourceList,
    _In_ WDFCMRESLIST ResourceListTranslated
    )
{
    NTSTATUS status;
    PDEVICE_CONTEXT pDeviceContext;
    WDF_USB_DEVICE_CREATE_CONFIG createParams;
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS configParams;
	UCHAR index, numberConfiguredPipes;
	WDF_USB_PIPE_INFORMATION pipeInfo;
	WDFUSBPIPE pipe;

    UNREFERENCED_PARAMETER(ResourceList);
    UNREFERENCED_PARAMETER(ResourceListTranslated);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    status = STATUS_SUCCESS;
    pDeviceContext = DeviceGetContext(Device);

    //
    // Create a USB device handle so that we can communicate with the
    // underlying USB stack. The WDFUSBDEVICE handle is used to query,
    // configure, and manage all aspects of the USB device.
    // These aspects include device properties, bus properties,
    // and I/O creation and synchronization. We only create the device the first time
    // PrepareHardware is called. If the device is restarted by pnp manager
    // for resource rebalance, we will use the same device handle but then select
    // the interfaces again because the USB stack could reconfigure the device on
    // restart.
    //
    if (pDeviceContext->UsbDevice == NULL) {

        //
        // Specifying a client contract version of 602 enables us to query for
        // and use the new capabilities of the USB driver stack for Windows 8.
        // It also implies that we conform to rules mentioned in MSDN
        // documentation for WdfUsbTargetDeviceCreateWithParameters.
        //
        WDF_USB_DEVICE_CREATE_CONFIG_INIT(&createParams,
                                         USBD_CLIENT_CONTRACT_VERSION_602
                                         );

        status = WdfUsbTargetDeviceCreateWithParameters(Device,
                                                    &createParams,
                                                    WDF_NO_OBJECT_ATTRIBUTES,
                                                    &pDeviceContext->UsbDevice
                                                    );

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
                        "WdfUsbTargetDeviceCreateWithParameters failed 0x%x", status);
			goto exit;
        }
    }

    //
    // Select the first interface
	//
	WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&configParams);
    status = WdfUsbTargetDeviceSelectConfig(pDeviceContext->UsbDevice,
		WDF_NO_OBJECT_ATTRIBUTES,
		&configParams
	);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
			"WdfUsbTargetDeviceSelectConfig failed 0x%x", status);
		goto exit;
    }

	//
	// Retrieve pipe information
	//
	pDeviceContext->UsbInterface = configParams.Types.SingleInterface.ConfiguredUsbInterface;
	numberConfiguredPipes = configParams.Types.SingleInterface.NumberConfiguredPipes;
	for (index = 0; index < numberConfiguredPipes; index++) {
		WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);
		pipe = WdfUsbInterfaceGetConfiguredPipe(
			pDeviceContext->UsbInterface,
			index, //PipeIndex,
			&pipeInfo
		);

		//
		// Tell the framework that it's okay to read less than
		// MaximumPacketSize
		//
		WdfUsbTargetPipeSetNoMaximumPacketSizeCheck(pipe);

		switch (pipeInfo.PipeType) {
		case WdfUsbPipeTypeBulk:
			if (USB_ENDPOINT_DIRECTION_IN(pipeInfo.EndpointAddress))
			{
				pDeviceContext->BulkPipeIn = pipe;
			}
			else if (USB_ENDPOINT_DIRECTION_OUT(pipeInfo.EndpointAddress))
			{
				pDeviceContext->BulkPipeOut = pipe;
			}
			
			break;
		}
	}

	//
	// Validate pipe
	//
	if (!pDeviceContext->BulkPipeIn || !pDeviceContext->BulkPipeOut) {
		status = STATUS_INVALID_DEVICE_STATE;
		TraceEvents(
			TRACE_LEVEL_ERROR, TRACE_DEVICE,
			"%!FUNC! Device is not configured properly %!STATUS!",
			status
		);

		goto exit;
	}

exit:
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");
    return status;
}

NTSTATUS
DFRDisplayEvtDeviceD0Entry(
	_In_ WDFDEVICE Device,
	_In_ WDF_POWER_DEVICE_STATE PreviousState
)
{
	NTSTATUS Status;

	DFR_INFORMATION DfrInformation;	
	ULONG PipeBytesTransferrred = 0;
	ULONG RetryCount = 0;

	PDEVICE_CONTEXT pDeviceContext;

	TraceEvents(
		TRACE_LEVEL_INFORMATION, TRACE_DRIVER,
		"%!FUNC! -->AmtPtpDeviceEvtDeviceD0Entry - coming from %s",
		DbgDevicePowerString(PreviousState)
	);

	pDeviceContext = DeviceGetContext(Device);

	// Reset all USB pipes.
	Status = WdfUsbTargetPipeResetSynchronously(
		pDeviceContext->BulkPipeOut,
		NULL,
		NULL
	);

	if (!NT_SUCCESS(Status)) {
		TraceEvents(
			TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC! WdfUsbTargetPipeResetSynchronously with %!STATUS!",
			Status
		);
		goto exit;
	}

	Status = WdfUsbTargetPipeResetSynchronously(
		pDeviceContext->BulkPipeIn,
		NULL,
		NULL
	);

	if (!NT_SUCCESS(Status)) {
		TraceEvents(
			TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC! WdfUsbTargetPipeResetSynchronously with %!STATUS!",
			Status
		);
		goto exit;
	}

	// Request iBridge Display info and validate
	Status = DFRDisplaySendGenericRequestAndForget(
		pDeviceContext,
		DFR_GET_INFO_KEY
	);

	if (!NT_SUCCESS(Status)) {
		TraceEvents(
			TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC! DFRDisplaySendGenericRequestAndForget with %!STATUS!",
			Status
		);
		goto exit;
	}

get_info:
	RtlZeroMemory(&DfrInformation, sizeof(DfrInformation));
	PipeBytesTransferrred = 0;

	Status = DFRDisplayGetBufferSynchronously(
		pDeviceContext,
		&DfrInformation,
		sizeof(DfrInformation),
		&PipeBytesTransferrred
	);

	if (!NT_SUCCESS(Status)) {
		TraceEvents(
			TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC! DFRDisplayGetBufferSynchronously with %!STATUS!",
			Status
		);
		goto exit;
	}

	// This could fail - because it is not quite ready
	// Just go ahead and retry - it should up later
	if (PipeBytesTransferrred < sizeof(DfrInformation)) {
		if (RetryCount < DFR_INIT_GINF_RETRY_MAX) {
			TraceEvents(
				TRACE_LEVEL_WARNING, TRACE_DRIVER,
				"%!FUNC! DFRDisplayGetBufferSynchronously buffer too small for GINF, entry retry."
			);
			RetryCount++;
			goto get_info;
		} else {
			TraceEvents(
				TRACE_LEVEL_ERROR, TRACE_DRIVER,
				"%!FUNC! DFRDisplayGetBufferSynchronously buffer too small for GINF"
			);
			Status = STATUS_DEVICE_DATA_ERROR;
			goto exit;
		}
	}

	if (DFR_DEVICE_RESPONSE_HEADER != DfrInformation.ResponseHeader ||
		DFR_GET_INFO_KEY != DfrInformation.ResponseMessageKey) {
		TraceEvents(
			TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC! Display GINF returned invalid message"
		);
		Status = STATUS_DEVICE_DATA_ERROR;
		goto exit;
	}

	if (DFR_FRAMEBUFFER_FORMAT != DfrInformation.PixelFormat) {
		TraceEvents(
			TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC! Display reported unsupported FrameBuffer format"
		);
		Status = STATUS_INVALID_PARAMETER;
		goto exit;
	}

	pDeviceContext->Width = (UINT16) DfrInformation.Width;
	pDeviceContext->Height = (UINT16) DfrInformation.Height;
	pDeviceContext->FrameBufferPixelFormat = DfrInformation.PixelFormat;

	// Notify iBridge Display that host is ready
	Status = DFRDisplaySendGenericRequestAndForget(
		pDeviceContext,
		DFR_HOST_READY_KEY
	);

	if (!NT_SUCCESS(Status)) {
		TraceEvents(
			TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC! DFRDisplaySendGenericRequestAndForget with %!STATUS!",
			Status
		);
		goto exit;
	}

	// This command is intended to clear the display
	// But I found that it doesn't quite work
	// Sent anyway as macOS did so
	Status = DFRDisplaySendGenericRequestAndForget(
		pDeviceContext,
		DFR_CLEAR_SCREEN_KEY
	);

	if (!NT_SUCCESS(Status)) {
		TraceEvents(
			TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC! DFRDisplaySendGenericRequestAndForget with %!STATUS!",
			Status
		);
		goto exit;
	}

	// The iBridge Display is now ready to receive FrameBuffer updates.
	pDeviceContext->DeviceReady = TRUE;

	// Clear the FrameBuffer if necessary
	if (!pDeviceContext->DeviceScreenCleared) {
		Status = DFRDisplayClear(Device);
		pDeviceContext->DeviceScreenCleared = TRUE;
	}

exit:
	return Status;
}

NTSTATUS
DFRDisplayEvtDeviceD0Exit(
	_In_ WDFDEVICE Device,
	_In_ WDF_POWER_DEVICE_STATE TargetState
)
{
	PDEVICE_CONTEXT pDeviceContext = DeviceGetContext(Device);

	TraceEvents(
		TRACE_LEVEL_INFORMATION, TRACE_DRIVER,
		"%!FUNC! -->AmtPtpDeviceEvtDeviceD0Exit - moving to %s",
		DbgDevicePowerString(TargetState)
	);

	pDeviceContext->DeviceReady = FALSE;

	return STATUS_SUCCESS;
}
