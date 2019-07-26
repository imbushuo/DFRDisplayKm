#include <Driver.h>
#include "DfrTransport.tmh"

NTSTATUS
DFRDisplaySendBufferSynchronously(
	_In_ PDEVICE_CONTEXT pDeviceContext,
	_In_ PVOID Buffer,
	_In_ ULONG BufferSize
)
{
	NTSTATUS Status = STATUS_SUCCESS;

	if (NULL == pDeviceContext || NULL == Buffer || 0 == BufferSize) {
		Status = STATUS_INVALID_PARAMETER;
		goto exit;
	}

	WDF_MEMORY_DESCRIPTOR BufferInDescriptor;
	ULONG PipeBytesTransferrred = 0;

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
		&BufferInDescriptor,
		Buffer,
		BufferSize
	);

	Status = WdfUsbTargetPipeWriteSynchronously(
		pDeviceContext->BulkPipeOut,
		NULL,
		NULL,
		&BufferInDescriptor,
		&PipeBytesTransferrred
	);

exit:
	return Status;
}

NTSTATUS
DFRDisplayGetBufferSynchronously(
	_In_ PDEVICE_CONTEXT pDeviceContext,
	_In_ PVOID Buffer,
	_In_ ULONG BufferSize,
	_Out_ PULONG PipeBytesTransferrred
)
{
	NTSTATUS Status = STATUS_SUCCESS;

	if (NULL == pDeviceContext || NULL == Buffer || 0 == BufferSize) {
		Status = STATUS_INVALID_PARAMETER;
		goto exit;
	}

	WDF_MEMORY_DESCRIPTOR BufferDescriptor;
	ULONG Transferrred = 0;

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
		&BufferDescriptor,
		(PVOID)Buffer,
		BufferSize
	);

	Status = WdfUsbTargetPipeReadSynchronously(
		pDeviceContext->BulkPipeIn,
		NULL,
		NULL,
		&BufferDescriptor,
		&Transferrred
	);

	if (!NT_SUCCESS(Status)) {
		TraceEvents(
			TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC! WdfUsbTargetPipeReadSynchronously with %!STATUS!",
			Status
		);
		goto exit;
	}

	if (NULL != PipeBytesTransferrred) {
		*PipeBytesTransferrred = Transferrred;
	}

exit:
	return Status;
}

NTSTATUS
DFRDisplaySendGenericRequestAndForget(
	_In_ PDEVICE_CONTEXT pDeviceContext,
	_In_ UINT32 RequestKey
)
{
	NTSTATUS Status = STATUS_SUCCESS;
	DFR_GENERIC_REQUEST DfrRequest;

	if (NULL == pDeviceContext) {
		Status = STATUS_INVALID_PARAMETER;
		goto exit;
	}

	RtlZeroMemory(&DfrRequest, sizeof(DfrRequest));
	DfrRequest.Header.RequestHeader = DFR_DEVICE_REQUEST_HEADER;
	DfrRequest.Header.RequestLength = sizeof(DfrRequest.Content);
	DfrRequest.Content.RequestKey = RequestKey;
	DfrRequest.Content.End = DFR_GENERIC_REQUEST_END;

	Status = DFRDisplaySendBufferSynchronously(
		pDeviceContext,
		&DfrRequest,
		sizeof(DfrRequest)
	);

	if (!NT_SUCCESS(Status)) {
		TraceEvents(
			TRACE_LEVEL_ERROR, TRACE_DRIVER,
			"%!FUNC! DFRDisplaySendBufferSynchronously with %!STATUS!",
			Status
		);
		goto exit;
	}

exit:
	return Status;
}
