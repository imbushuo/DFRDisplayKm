/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

//
// Define an Interface Guid so that app can find the device and talk to it.
//

DEFINE_GUID (GUID_DEVINTERFACE_DFRDisplayKm,
    0x2003cacd,0x9e7c,0x477c,0xab,0x06,0xa5,0xa8,0xbb,0xb1,0xa6,0x3e);
// {2003cacd-9e7c-477c-ab06-a5a8bbb1a63e}
