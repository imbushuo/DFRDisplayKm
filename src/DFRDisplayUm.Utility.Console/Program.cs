using DFRDisplayUm.Utility.Console.Interop;
using System;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;

namespace DFRDisplayUm.Utility.Console
{
    class Program
    {
        static bool ClearDfrFrameBuffer(IntPtr deviceHandle)
        {
            return IoCtl.DeviceIoControl(
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

        static unsafe bool DrawBitmap(IntPtr deviceHandle, string file, ushort x, ushort y)
        {
            ushort width = 0, height = 0;
            var bResult = false;

            try
            {
                using (var bitmap = new Bitmap(file))
                {
                    if (bitmap.Width > ushort.MaxValue || bitmap.Height > ushort.MaxValue)
                    {
                        System.Console.Write("Image too large");
                        return false;
                    }

                    width = (ushort)bitmap.Width;
                    height = (ushort)bitmap.Height;

                    int requestSize = bitmap.Width * bitmap.Height * 3 +
                        Marshal.SizeOf(typeof(DFR_HOSTIO_UPDATE_FRAMEBUFFER_HEADER));

                    // Memory stream for FrameBuffer update request
                    IntPtr requestPtr = Marshal.AllocHGlobal(requestSize);
                    if (requestPtr == IntPtr.Zero)
                    {
                        System.Console.Write("Failed to allocate memory for FrameBuffer");
                        return false;
                    }

                    byte* requestBytePtr = (byte*)requestPtr.ToPointer();
                    UnmanagedMemoryStream requestStream = new UnmanagedMemoryStream(requestBytePtr, requestSize, requestSize, FileAccess.Write);

                    using (var binaryWriter = new BinaryWriter(requestStream))
                    {
                        binaryWriter.Write(x);
                        binaryWriter.Write(y);
                        binaryWriter.Write(width);
                        binaryWriter.Write(height);
                        binaryWriter.Write(DfrHostIo.DFR_FRAMEBUFFER_FORMAT);
                        binaryWriter.Write((uint)0);

                        for (int w = 0; w < bitmap.Width; w++)
                        {
                            for (int h = bitmap.Height - 1; h >= 0; h--)
                            {
                                var pixel = bitmap.GetPixel(w, h);
                                binaryWriter.Write(pixel.R);
                                binaryWriter.Write(pixel.G);
                                binaryWriter.Write(pixel.B);
                            }
                        }

                        binaryWriter.Flush();

                        bResult = IoCtl.DeviceIoControl(
                            deviceHandle,
                            DfrHostIo.IOCTL_DFR_UPDATE_FRAMEBUFFER,
                            requestPtr,
                            requestSize,
                            IntPtr.Zero,
                            0,
                            IntPtr.Zero,
                            IntPtr.Zero
                        );
                    }

                    Marshal.FreeHGlobal(requestPtr);
                }
            }
            catch (Exception exc)
            {
                System.Console.Write($"Exception caught: {exc}");
            }

            return bResult;
        }

        static void ShowHelp()
        {
            System.Console.WriteLine("Usage: DFRDisplayUm.Utility.Console.exe [Action] [Image Path] [X] [Y]");
            System.Console.WriteLine("Available Actions: clear draw");
            System.Console.WriteLine();
            System.Console.WriteLine("Action draw:");
            System.Console.WriteLine("Image should have size less than 2170 * 60 or it will be rejected by the DFR driver.");
            System.Console.WriteLine("X and Y axis is optional.");
        }

        unsafe static void Main(string[] args)
        {
            if (args.Length < 1)
            {
                ShowHelp();
                return;
            }

            // Find DFR Device
            var instancePath = DfrDeviceDiscovery.FindDfrDevice();
            if (string.IsNullOrEmpty(instancePath))
            {
                System.Console.WriteLine("No DFR device found");
                return;
            }

            System.Console.WriteLine($"Found DFR instance {instancePath}");
            var deviceHandle = IoCtl.CreateFile(
                instancePath, FileAccess.Write, FileShare.None,
                IntPtr.Zero, FileMode.Open, FileOptions.None,
                IntPtr.Zero
            );

            if (deviceHandle == IntPtr.Zero)
            {
                System.Console.WriteLine("Failed to open DFR device");
                return;
            }

            bool bResult = false;
            bool commandFound = true;
            switch (args[0].ToLowerInvariant())
            {
                case "clear":
                    bResult = ClearDfrFrameBuffer(deviceHandle);
                    break;
                case "draw":
                    ushort x = 0, y = 0;
                    // Optional X
                    if (args.Length >= 3)
                    {
                        if (ushort.TryParse(args[2], out ushort tX))
                        {
                            x = tX;
                        }
                    }

                    // Optional Y
                    if (args.Length >= 4)
                    {
                        if (ushort.TryParse(args[3], out ushort tY))
                        {
                            y = tY;
                        }
                    }

                    bResult = DrawBitmap(deviceHandle, args[1], x, y);
                    break;
                default:
                    ShowHelp();
                    commandFound = false;
                    break;
            }

            if (!bResult && commandFound)
            {
                System.Console.WriteLine("Failed to run the command");
            }

            IoCtl.CloseHandle(deviceHandle);
        }
    }
}
