using System;
using System.IO;
using System.Runtime.InteropServices;

namespace DFRDisplayUm.Interop
{
    public static class IoCtl
    {
        [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        public static extern IntPtr CreateFile(
            string filename,
            FileAccess access,
            FileShare sharing,
            IntPtr SecurityAttributes,
            FileMode mode,
            FileOptions options,
            IntPtr template
        );

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool DeviceIoControl(
            IntPtr device, 
            uint ctlcode,
            IntPtr inbuffer,
            int inbuffersize,
            IntPtr outbuffer,
            int outbufferSize,
            IntPtr bytesreturned,
            IntPtr overlapped
        );

        [DllImport("kernel32.dll")]
        public static extern void CloseHandle(IntPtr hdl);

        public static bool ClearDfrFrameBuffer(IntPtr deviceHandle)
        {
            return DeviceIoControl(
                deviceHandle,
                DfrHostIo.IOCTL_DFR_CLEAR_FRAMEBUFFER,
                IntPtr.Zero,
                0,
                IntPtr.Zero,
                0,
                IntPtr.Zero,
                IntPtr.Zero
            );
        }
    }
}
