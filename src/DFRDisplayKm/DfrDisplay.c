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
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

NTSTATUS
DFRDisplayClear(
	_In_ WDFDEVICE Device
)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PDEVICE_CONTEXT pDeviceContext = DeviceGetContext(Device);

	WDFMEMORY FrameBufferMemory = NULL;
	PUCHAR FrameBuffer;
	size_t FrameBufferLength;

	if (FALSE == pDeviceContext->DeviceReady) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"Device is not yet ready"
		);
		Status = STATUS_DEVICE_NOT_READY;
		goto exit;
	}

	FrameBufferLength = (size_t) pDeviceContext->Width * 
		pDeviceContext->Height * DFR_FRAMEBUFFER_PIXEL_BYTES;
	Status = WdfMemoryCreate(
		WDF_NO_OBJECT_ATTRIBUTES,
		NonPagedPool,
		POOL_TAG,
		FrameBufferLength,
		&FrameBufferMemory,
		&FrameBuffer
	);

	if (!NT_SUCCESS(Status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"WdfMemoryCreate failed with %!STATUS!",
			Status
		);
		goto exit;
	}

	// So it should be all black
	RtlZeroMemory(FrameBuffer, FrameBufferLength);
	Status = DFRDisplayTransferFrameBuffer(
		Device,
		0,
		0,
		pDeviceContext->Width,
		pDeviceContext->Height,
		FrameBuffer,
		FrameBufferLength
	);

	if (!NT_SUCCESS(Status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"DFRDisplayTransferFrameBuffer failed with %!STATUS!",
			Status
		);
		goto exit;
	}

exit:
	if (NULL != FrameBufferMemory) {
		WdfObjectDelete(FrameBufferMemory);
	}
	return Status;
}

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
	ULONG BytesTransferrred;

	DFR_FRAMEBUFFER_UPDATE_RESPONSE DfrUpdateResponse;
	UINT8 RetransmitCount = 0;

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

	FbUpdateRequest->Content.Reserved0 = DFR_DEVICE_UPDATE_FB_CONTENT_RSVD_VALUE_0;
	if (pDeviceContext->CurrentFrameId >= 0xff) {
		pDeviceContext->CurrentFrameId = 0x00;
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

retry:
	RequestOption.Timeout = -3000000;
	Status = WdfUsbTargetPipeWriteSynchronously(
		pDeviceContext->BulkPipeOut,
		NULL,
		&RequestOption,
		&RequestMemoryDescriptor,
		&BytesTransferrred
	);

	if (!NT_SUCCESS(Status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"WdfUsbTargetPipeWriteSynchronously failed with %!STATUS!",
			Status
		);
		goto exit;
	}

	RtlZeroMemory(&DfrUpdateResponse, sizeof(DfrUpdateResponse));
	Status = DFRDisplayGetBufferSynchronously(
		pDeviceContext,
		&DfrUpdateResponse,
		sizeof(DfrUpdateResponse),
		&BytesTransferrred
	);

	if (!NT_SUCCESS(Status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"DFRDisplayGetBufferSynchronously failed with %!STATUS!",
			Status
		);
		goto exit;
	}

	if (DFR_FRAMEBUFFER_UPDATED_KEY != DfrUpdateResponse.FrameId ||
		FbUpdateRequest->Content.FrameId != DfrUpdateResponse.FrameId) {
		if (DFR_FB_UPDATE_RETRANSMIT_MAX > RetransmitCount) {
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DFRDISP,
				"Retransmit frame %d",
				FbUpdateRequest->Content.FrameId
			);

			RetransmitCount++;
			goto retry;
		} else {
			TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
				"Retransmit timed out"
			);

			Status = STATUS_TIMEOUT;
			goto exit;
		}
	}

exit:
	if (NULL != RequestMemory) {
		WdfObjectDelete(RequestMemory);
	}
	return Status;
}

NTSTATUS
DFRDisplayHandleUsermodeBufferTransfer(
	_In_ WDFDEVICE Device,
	_In_ WDFREQUEST Request,
	_In_ size_t InputBufferLength,
	_Out_ BOOLEAN *RequestPending
)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PDEVICE_CONTEXT pDeviceContext = DeviceGetContext(Device);

	WDFMEMORY InputMemory;
	PUCHAR InputBuffer = NULL;
	size_t InputBufferSize;

	PDFR_HOSTIO_UPDATE_FRAMEBUFFER_HEADER HostFbUpdateRequestHeader;
	size_t FrameBufferLength;

	// By default not make request pending
	// TODO: Implement async BLT later
	*RequestPending = FALSE;

	// Check device status
	if (FALSE == pDeviceContext->DeviceReady) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"Device is not yet ready"
		);
		Status = STATUS_DEVICE_NOT_READY;
		goto exit;
	}

	// Absolutely not expect such request below:
	if (sizeof(DFR_HOSTIO_UPDATE_FRAMEBUFFER_HEADER) >= InputBufferLength) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"DFRDisplayUpdateFrameBuffer received invalid buffer length %lld",
			InputBufferLength
		);

		Status = STATUS_DATA_ERROR;
		goto exit;
	}

	// Retrieve IOCTL input
	Status = WdfRequestRetrieveInputMemory(
		Request,
		&InputMemory
	);

	if (!NT_SUCCESS(Status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"WdfRequestRetrieveInputMemory failed with %!STATUS!",
			Status
		);
		goto exit;
	}

	InputBuffer = WdfMemoryGetBuffer(
		InputMemory,
		&InputBufferSize
	);

	if (NULL == InputBuffer) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"WdfMemoryGetBuffer failed: InputBuffer == NULL"
		);
		goto exit;
	}

	HostFbUpdateRequestHeader = (PDFR_HOSTIO_UPDATE_FRAMEBUFFER_HEADER) InputBuffer;

	// Validate region
	if (HostFbUpdateRequestHeader->BeginX >= pDeviceContext->Width ||
		HostFbUpdateRequestHeader->BeginY >= pDeviceContext->Height) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"FrameBuffer Request out of screen boundary, start (%d, %d)",
			HostFbUpdateRequestHeader->BeginX,
			HostFbUpdateRequestHeader->BeginY
		);
		Status = STATUS_INVALID_PARAMETER;
		goto exit;
	}

	if (HostFbUpdateRequestHeader->BeginX + HostFbUpdateRequestHeader->Width > pDeviceContext->Width ||
		HostFbUpdateRequestHeader->BeginY + HostFbUpdateRequestHeader->Height > pDeviceContext->Height) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"FrameBuffer Request out of screen boundary, end (%d, %d)",
			HostFbUpdateRequestHeader->BeginX + HostFbUpdateRequestHeader->Width,
			HostFbUpdateRequestHeader->BeginY + HostFbUpdateRequestHeader->Height
		);
		Status = STATUS_INVALID_PARAMETER;
		goto exit;
	}

	// Validate Pixel format
	if (HostFbUpdateRequestHeader->FrameBufferPixelFormat != pDeviceContext->FrameBufferPixelFormat) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"FrameBuffer has unsupported pixel format %d",
			HostFbUpdateRequestHeader->FrameBufferPixelFormat
		);
		Status = STATUS_INVALID_PARAMETER;
		goto exit;
	}

	// Not yet support vertical flip
	// TODO: Implement vertical flip
	if (HostFbUpdateRequestHeader->RequireVertFlip) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"Vert Flip is not yet implemented"
		);
		Status = STATUS_NOT_IMPLEMENTED;
		goto exit;
	}

	// Validate size
	FrameBufferLength = (size_t) HostFbUpdateRequestHeader->Width * 
		HostFbUpdateRequestHeader->Height * DFR_FRAMEBUFFER_PIXEL_BYTES;
	if (FrameBufferLength + sizeof(DFR_HOSTIO_UPDATE_FRAMEBUFFER_HEADER) > InputBufferLength) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DFRDISP,
			"DFRDisplayUpdateFrameBuffer received invalid buffer length %lld",
			InputBufferLength);

		Status = STATUS_DATA_ERROR;
		goto exit;
	}

	// Transfer FrameBuffer
	Status = DFRDisplayTransferFrameBuffer(
		Device,
		HostFbUpdateRequestHeader->BeginX,
		HostFbUpdateRequestHeader->BeginY,
		HostFbUpdateRequestHeader->Width,
		HostFbUpdateRequestHeader->Height,
		(PVOID)(InputBuffer + sizeof(DFR_HOSTIO_UPDATE_FRAMEBUFFER_HEADER)),
		FrameBufferLength
	);

exit:
	return Status;
}