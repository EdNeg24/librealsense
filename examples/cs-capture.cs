﻿using System;
using System.Drawing;
using System.Windows.Forms;
using System.Runtime.InteropServices;

public class Capture : Form
{
    private RealSense.Camera camera;
    private PictureBox color, depth;
    private Timer timer;

    public Capture(RealSense.Camera camera)
    {
        this.camera = camera;

        var colorIntrin = camera.GetStreamIntrinsics(RealSense.Stream.Color);
        var depthIntrin = camera.GetStreamIntrinsics(RealSense.Stream.Depth);
            
        Text = string.Format("C# Capture Example ({0})", camera.Name);
        ClientSize = new System.Drawing.Size(colorIntrin.ImageSize[0] + depthIntrin.ImageSize[0] + 36, colorIntrin.ImageSize[1] + 24);

        color = new PictureBox { Location = new Point(12, 12), Size = new Size(colorIntrin.ImageSize[0], colorIntrin.ImageSize[1]) };
        Controls.Add(color);

        depth = new PictureBox { Location = new Point(24 + colorIntrin.ImageSize[0], 12), Size = new Size(depthIntrin.ImageSize[0], depthIntrin.ImageSize[1]) };
        Controls.Add(depth);

        timer = new Timer { Interval = 16 };
        timer.Tick += this.OnCaptureTick;
        timer.Start();
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) timer.Dispose();
        base.Dispose(disposing);
    }

    private void OnCaptureTick(object sender, EventArgs e)
    {
        if (camera == null) return;

        timer.Stop();

        camera.WaitAllStreams();

        // Obtain color image data
        RealSense.Intrinsics intrinsics = camera.GetStreamIntrinsics(RealSense.Stream.Color);
        int width = intrinsics.ImageSize[0], height = intrinsics.ImageSize[1];

        // Create a bitmap of the color image and assign it to our PictureBox
        color.Image = new Bitmap(width, height, width * 3, System.Drawing.Imaging.PixelFormat.Format24bppRgb, camera.GetImagePixels(RealSense.Stream.Color));

        // Obtain depth image data
        intrinsics = camera.GetStreamIntrinsics(RealSense.Stream.Depth);
        width = intrinsics.ImageSize[0];
        height = intrinsics.ImageSize[1];            

        // Build a cumulative histogram for of depth values in [1,0xFFFF]
        var depthPixels = new short[width*height];
        Marshal.Copy(camera.GetImagePixels(RealSense.Stream.Depth), depthPixels, 0, depthPixels.Length);

        var histogram = new uint[0x10000];
        for (int i = 0; i < width * height; ++i) ++histogram[(ushort)depthPixels[i]];
        for(int i = 2; i < 0x10000; ++i) histogram[i] += histogram[i-1]; 

        // Produce an image in BGR ordered byte array
        var image = new byte[width * height * 3];
        for(int i = 0; i < width*height; ++i)
        {
            ushort d = (ushort)depthPixels[i];
            if(d != 0)
            {
                uint f = histogram[d] * 255 / histogram[0xFFFF]; // 0-255 based on histogram location
                image[i*3 + 0] = (byte)f;
                image[i*3 + 1] = 0;
                image[i*3 + 2] = (byte)(255 - f);
            }
            else
            {
                image[i*3 + 0] = 0;
                image[i*3 + 1] = 5;
                image[i*3 + 2] = 20;
            }
        }

        // Create a bitmap of the histogram colored depth image and assign it to our PictureBox
        GCHandle handle = GCHandle.Alloc(image, GCHandleType.Pinned);
        depth.Image = new Bitmap(width, height, width * 3, System.Drawing.Imaging.PixelFormat.Format24bppRgb, handle.AddrOfPinnedObject());
        handle.Free();

        timer.Start();
    }

    static void Main()
    {
        try
        {
            using (var context = new RealSense.Context())
            {
                var cameras = context.Cameras;
                if (cameras.Length < 1)
                {
                    MessageBox.Show("No RealSense cameras detected. Program will now exit.", "C# Capture Example", MessageBoxButtons.OK);
                    return;
                }

                context.Cameras[0].EnableStreamPreset(RealSense.Stream.Depth, RealSense.Preset.BestQuality);
                context.Cameras[0].EnableStream(RealSense.Stream.Color, 640, 480, RealSense.Format.BGR8, 60);
                context.Cameras[0].StartCapture();

                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.Run(new Capture(context.Cameras[0]));
            }
        }
        catch (RealSense.Error e)
        {
            MessageBox.Show(string.Format("RealSense error calling {0}({1}):\n  {2}\nProgram will now exit.",
                e.FailedFunction, e.FailedArgs, e.Message), "C# Capture Example", MessageBoxButtons.OK);
        }
    }
}
