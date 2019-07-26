#include <Driver.h>
#include "DfrDisplay.tmh"

const UCHAR DfrUpdatePadding[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xFE, 0xFF, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x08, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};

NTSTATUS
DFRDisplayTransferFrameBuffer(
	_In_ WDFDEVICE Device,
	_In_ UINT16 BeginPosX,
	_In_ UINT16 BeginPosY,
	_In_ UINT16 Width,
	_In_ UINT16 Height,
	_In_ PVOID FrameBuffer,
	_In_ size_t FrameBufferLength
)
// Internal comsuption - most parameters are not checked
{
	NTSTATUS Status = STATUS_SUCCESS;
	PDEVICE_CONTEXT pDeviceContext = DeviceGetContext(Device);
	
	WDFMEMORY RequestMemory = NULL;
	PUCHAR RequestBuffer;
	size_t RequestBufferLength;
	PDFR_UPDATE_FB_REQUEST FbUpdateRequest;
	WDF_MEMORY_DESCRIPTOR RequestMemoryDescriptor;

	WDF_REQUEST_SEND_OPTIONS RequestOption;
	ULONG BytesWritten;

	// Check device status
	if (FALSE == pDeviceContext->DeviceReady) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"Device is not yet ready"
		);
		Status = STATUS_DEVICE_NOT_READY;
		goto exit;
	}

	RequestBufferLength = FrameBufferLength + sizeof(DFR_UPDATE_FB_REQUEST) + sizeof(DfrUpdatePadding);
	Status = WdfMemoryCreate(
		WDF_NO_OBJECT_ATTRIBUTES,
		NonPagedPool,
		POOL_TAG,
		RequestBufferLength,
		&RequestMemory,
		&RequestBuffer
	);

	if (!NT_SUCCESS(Status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"WdfMemoryCreate failed with %!STATUS!",
			Status
		);
		goto exit;
	}

	RtlZeroMemory(RequestBuffer, RequestBufferLength);
	FbUpdateRequest = (PDFR_UPDATE_FB_REQUEST) RequestBuffer;

	// Header
	FbUpdateRequest->Header.RequestHeader = DFR_DEVICE_UPDATE_FB_REQUEST_HEADER;
	FbUpdateRequest->Header.Reserved1 = DFR_DEVICE_UPDATE_FB_REQUEST_RSVD_VALUE_1;
	FbUpdateRequest->Header.RequestLength = (UINT32) RequestBufferLength - sizeof(DFR_GENERIC_REQUEST_HEADER);

	if (pDeviceContext->CurrentFrameId >= 0xffffffff) {
		pDeviceContext->CurrentFrameId = 0x00000000;
	}

	FbUpdateRequest->Content.BeginX = BeginPosX;
	FbUpdateRequest->Content.BeginY = BeginPosY;
	FbUpdateRequest->Content.BufferSize = (UINT32) FrameBufferLength;
	FbUpdateRequest->Content.Width = Width;
	FbUpdateRequest->Content.Height = Height;
	FbUpdateRequest->Content.FrameId = ++pDeviceContext->CurrentFrameId;

	// Content
	RtlCopyMemory(
		&RequestBuffer[sizeof(DFR_UPDATE_FB_REQUEST)],
		FrameBuffer,
		FrameBufferLength
	);

	// Padding
	RtlCopyMemory(
		&RequestBuffer[sizeof(DFR_UPDATE_FB_REQUEST) + FrameBufferLength],
		DfrUpdatePadding,
		sizeof(DfrUpdatePadding)
	);

	WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(
		&RequestMemoryDescriptor,
		RequestMemory,
		NULL
	);

	WDF_REQUEST_SEND_OPTIONS_INIT(
		&RequestOption,
		WDF_REQUEST_SEND_OPTION_TIMEOUT
	);

	RequestOption.Timeout = -1000000;
	Status = WdfUsbTargetPipeWriteSynchronously(
		pDeviceContext->BulkPipeOut,
		NULL,
		&RequestOption,
		&RequestMemoryDescriptor,
		&BytesWritten
	);

	if (!NT_SUCCESS(Status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"WdfUsbTargetPipeWriteSynchronously failed with %!STATUS!",
			Status
		);
		goto exit;
	}

exit:
	if (NULL != RequestMemory) {
		WdfObjectDelete(RequestMemory);
	}
	return Status;
}