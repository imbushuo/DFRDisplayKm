using System;
using System.Runtime.InteropServices;

namespace DFRDisplayUm.Utility.Console.Interop
{
    class DfrHostIo
    {
        public const uint IOCTL_DFR_UPDATE_FRAMEBUFFER = 0x8086a004;
        public const uint IOCTL_DFR_CLEAR_FRAMEBUFFER = 0x8086a008;
        public const uint DFR_FRAMEBUFFER_FORMAT = 0x52474241;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct DFR_HOSTIO_UPDATE_FRAMEBUFFER_HEADER
    {
        public ushort BeginX;
        public ushort BeginY;
        public ushort Width;
        public ushort Height;
        public uint FrameBufferPixelFormat;
        public uint RequireVertFlip;
    }

    class DfrDeviceDiscovery
    {
        public static string FindDfrDevice()
        {
            string instancePath = null;
            var bResult = true;
            uint i = 0;

            var h = SetupAPI.SetupDiGetClassDevs(
                ref DeviceGuids.DfrDisplayInterfaceGuid,
                IntPtr.Zero,
                IntPtr.Zero,
                SetupAPI.DIGCF_PRESENT | SetupAPI.DIGCF_DEVICEINTERFACE
            );

            if (h != IntPtr.Zero)
            {
                // https://www.pinvoke.net/default.aspx/setupapi.setupdienumdeviceinterfaces
                while (bResult)
                {
                    SP_DEVICE_INTERFACE_DATA dia = new SP_DEVICE_INTERFACE_DATA();
                    dia.cbSize = Marshal.SizeOf(dia);

                    bResult = SetupAPI.SetupDiEnumDeviceInterfaces(h, IntPtr.Zero,
                        ref DeviceGuids.DfrDisplayInterfaceGuid, i, ref dia);
                    if (bResult)
                    {
                        SP_DEVINFO_DATA da = new SP_DEVINFO_DATA();
                        da.cbSize = Marshal.SizeOf(da);

                        SP_DEVICE_INTERFACE_DETAIL_DATA didd = new SP_DEVICE_INTERFACE_DETAIL_DATA
                        {
                            cbSize = 4 + Marshal.SystemDefaultCharSize // trust me :)
                        };

                        uint nRequiredSize = 0;
                        uint nBytes = 260;
                        if (SetupAPI.SetupDiGetDeviceInterfaceDetail(h, ref dia, ref didd, nBytes,
                            ref nRequiredSize, ref da))
                        {
                            instancePath = didd.DevicePath;
                            break;
                        }
                    }

                    i++;
                }
            }

            return instancePath;
        }
    }
}
