using DFRDisplayUm.Utility.Console.Interop;
using System;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;

namespace DFRDisplayUm.Utility.Console
{
    class Program
    {
        unsafe static void Main(string[] args)
        {
            if (args.Length < 1)
            {
                System.Console.WriteLine("Usage: DFRDisplayUm.Utility.Console.exe [Image Path] [X] [Y]");
                System.Console.WriteLine("Image should have size less than 2170 * 60 or it will be rejected by the DFR driver.");
                System.Console.WriteLine("X and Y axis is optional.");
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

            ushort x = 0, y = 0, width = 0, height = 0;

            // Optional X
            if (args.Length >= 2)
            {
                if (ushort.TryParse(args[1], out ushort tX))
                {
                    x = tX;
                }
            }

            // Optional Y
            if (args.Length >= 3)
            {
                if (ushort.TryParse(args[2], out ushort tY))
                {
                    y = tY;
                }
            }

            try
            {
                using (var bitmap = new Bitmap(args[0]))
                {
                    if (bitmap.Width > ushort.MaxValue || bitmap.Height > ushort.MaxValue)
                    {
                        System.Console.Write("Image too large");
                        return;
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
                        return;
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

                        var result = IoCtl.DeviceIoControl(
                            deviceHandle,
                            DfrHostIo.IOCTL_DFR_UPDATE_FRAMEBUFFER,
                            requestPtr,
                            requestSize,
                            IntPtr.Zero,
                            0,
                            IntPtr.Zero,
                            IntPtr.Zero
                        );

                        if (!result)
                        {
                            System.Console.WriteLine("IOCTL to DFR failed");
                        }
                        else
                        {
                            System.Console.WriteLine("FrameBuffer copied");
                        }
                    }

                    Marshal.FreeHGlobal(requestPtr);
                }
            }
            catch (Exception exc)
            {
                System.Console.Write($"Failed to open file: {exc}");
            }

            IoCtl.CloseHandle(deviceHandle);
        }
    }
}
