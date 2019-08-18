#include <Driver.h>
#include "queue.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, DFRDisplayKmQueueInitialize)
#endif

NTSTATUS
DFRDisplayKmQueueInitialize(
    _In_ WDFDEVICE Device
    )
{
    WDFQUEUE queue;
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG queueConfig;
	PDEVICE_CONTEXT pDeviceContext;

    PAGED_CODE();
	pDeviceContext = DeviceGetContext(Device);
    
    //
    // Configure a default queue so that requests that are not
    // configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
    // other queues get dispatched here.
    //
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoDeviceControl = DFRDisplayKmEvtIoDeviceControl;
    queueConfig.EvtIoStop = DFRDisplayKmEvtIoStop;

    status = WdfIoQueueCreate(
		Device,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&queue
	);

    if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, 
			"WdfIoQueueCreate failed %!STATUS!", status);
        return status;
    }

	WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);
	queueConfig.PowerManaged = WdfFalse;

	status = WdfIoQueueCreate(
		Device,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&pDeviceContext->FnKeyStatusQueue
	);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
			"WdfIoQueueCreate failed %!STATUS!", status);
	}

    return status;
}

VOID
DFRDisplayKmEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
{
	UNREFERENCED_PARAMETER(Queue);
	UNREFERENCED_PARAMETER(OutputBufferLength);

	NTSTATUS Status;

	WDFDEVICE Device = WdfIoQueueGetDevice(Queue);
	PDEVICE_CONTEXT pDeviceContext = DeviceGetContext(Device);
	BOOLEAN RequestPending = FALSE;

	switch (IoControlCode) {
	case IOCTL_DFR_UPDATE_FRAMEBUFFER:
		Status = DFRDisplayHandleUsermodeBufferTransfer(
			Device,
			Request,
			InputBufferLength,
			&RequestPending
		);
		break;
	case IOCTL_DFR_CLEAR_FRAMEBUFFER:
		Status = DFRDisplayClear(
			Device
		);
		break;
	case IOCTL_DFR_KBD_FN_PRESSED:
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "Fn key pressed");
		DFRDisplaySetFnStatus(Device, TRUE);
		Status = STATUS_SUCCESS;
		break;
	case IOCTL_DFR_KBD_FN_RELEASED:
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "Fn key released");
		DFRDisplaySetFnStatus(Device, FALSE);
		Status = STATUS_SUCCESS;
		break;
	case IOCTL_DFR_QUERY_FN_KEY:
		Status = WdfRequestForwardToIoQueue(
			Request,
			pDeviceContext->FnKeyStatusQueue
		);
		if (!NT_SUCCESS(Status)) {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE,
				"WdfRequestForwardToIoQueue failed %!STATUS!", Status);
			WdfRequestComplete(Request, Status);
		} else {
			RequestPending = TRUE;
		}
		break;
	default:
		Status = STATUS_NOT_SUPPORTED;
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "Unsupported IOCTL");
		break;
	}

	if (FALSE == RequestPending) {
		WdfRequestComplete(Request, Status);
	}
    return;
}

VOID
DFRDisplayKmEvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
)
{
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_QUEUE, 
                "%!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d", 
                Queue, Request, ActionFlags);

    //
    // In most cases, the EvtIoStop callback function completes, cancels, or postpones
    // further processing of the I/O request.
    //
    // Typically, the driver uses the following rules:
    //
    // - If the driver owns the I/O request, it either postpones further processing
    //   of the request and calls WdfRequestStopAcknowledge, or it calls WdfRequestComplete
    //   with a completion status value of STATUS_SUCCESS or STATUS_CANCELLED.
    //  
    //   The driver must call WdfRequestComplete only once, to either complete or cancel
    //   the request. To ensure that another thread does not call WdfRequestComplete
    //   for the same request, the EvtIoStop callback must synchronize with the driver's
    //   other event callback functions, for instance by using interlocked operations.
    //
    // - If the driver has forwarded the I/O request to an I/O target, it either calls
    //   WdfRequestCancelSentRequest to attempt to cancel the request, or it postpones
    //   further processing of the request and calls WdfRequestStopAcknowledge.
    //
    // A driver might choose to take no action in EvtIoStop for requests that are
    // guaranteed to complete in a small amount of time. For example, the driver might
    // take no action for requests that are completed in one of the driver’s request handlers.
    //

    return;
}
