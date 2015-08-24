#pragma once
#ifndef LIBREALSENSE_UVC_CAMERA_H
#define LIBREALSENSE_UVC_CAMERA_H

#include "rs-internal.h"

#ifdef USE_UVC_DEVICES
#include <libuvc/libuvc.h>

namespace rs
{
    const int MAX_STREAMS = 3;
    
    inline void CheckUVC(const char * call, uvc_error_t status)
    {
        if (status < 0)
        {
            throw std::runtime_error(ToString() << call << "(...) returned " << uvc_strerror(status));
        }
    }

    /*struct ResolutionMode
    {
        int stream;                 // RS_DEPTH, RS_COLOR, RS_INFRARED, RS_INFRARED_2, etc.
        int width, height, fps;     // Resolution and framerate visible to the library client
        int format;                 // Pixel format visible to the library client
        rs_intrinsics intrinsics;   // Image intrinsics
    };

    struct StreamMode
    {
        int subdevice;                      // 0, 1, 2, etc...
        int width, height, fps;             // Resolution and framerate advertised over UVC
        uvc_frame_format format;            // Pixel format advertised over UVC

        std::vector<ResolutionMode> images; // Resolution mode for images visible to the user
    };*/

    struct ResolutionMode
    {
        int stream;                 // RS_DEPTH, RS_COLOR, etc.

        int width, height;          // Resolution visible to the library client
        int fps;                    // Framerate visible to the library client
        int format;                 // Format visible to the library client

        int uvcWidth, uvcHeight;    // Resolution advertised over UVC
        int uvcFps;                 // Framerate advertised over UVC
        uvc_frame_format uvcFormat; // Format advertised over UVC

        rs_intrinsics intrinsics;   // Image intrinsics
    };

    // World's tiniest linear algebra library
    struct float3 { float x,y,z; float & operator [] (int i) { return (&x)[i]; } };
    struct float3x3 { float3 x,y,z; float & operator () (int i, int j) { return (&x)[j][i]; } }; // column-major
    struct pose { float3x3 orientation; float3 position; };
    inline float3 operator + (const float3 & a, const float3 & b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
    inline float3 operator * (const float3 & a, float b) { return {a.x*b, a.y*b, a.z*b}; }
    inline float3 operator * (const float3x3 & a, const float3 & b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
    inline float3x3 operator * (const float3x3 & a, const float3x3 & b) { return {a*b.x, a*b.y, a*b.z}; }
    inline float3x3 transpose(const float3x3 & a) { return {{a.x.x,a.y.x,a.z.x}, {a.x.y,a.y.y,a.z.y}, {a.x.z,a.y.z,a.z.z}}; }
    inline float3 operator * (const pose & a, const float3 & b) { return a.orientation * b + a.position; }
    inline pose operator * (const pose & a, const pose & b) { return {a.orientation * b.orientation, a.position + a * b.position}; }
    inline pose inverse(const pose & a) { auto inv = transpose(a.orientation); return {inv, inv * a.position * -1}; }

    struct CalibrationInfo
    {
        std::vector<ResolutionMode> modes;
        pose stream_poses[MAX_STREAMS];
        float depth_scale;
    };

    class UVCCamera : public rs_camera
    {
    protected: 
        class StreamInterface;
        class UserStreamInterface
        {
            friend class StreamInterface;

            ResolutionMode mode;

            volatile bool updated = false;
            std::vector<uint8_t> front, middle, back;
            std::mutex mutex;
        public:
            const ResolutionMode & get_mode() const { return mode; }
            const void * get_image() const { return front.data(); }

            void set_mode(const ResolutionMode & mode);
            bool update_image();
        };

        class StreamInterface
        {
            uvc_device_handle_t * uvcHandle;
            uvc_stream_ctrl_t ctrl;
            ResolutionMode mode;

            UserStreamInterface * user_interface;

            void on_frame(uvc_frame_t * frame);
        public:
            StreamInterface(uvc_device_t * device, int subdeviceNumber) { CheckUVC("uvc_open2", uvc_open2(device, &uvcHandle, subdeviceNumber)); }
            ~StreamInterface() { uvc_stop_streaming(uvcHandle); uvc_close(uvcHandle); }

            //const ResolutionMode & get_mode() const { return mode; }

            uvc_device_handle_t * get_handle() { return uvcHandle; }
            void set_mode(const ResolutionMode & mode);
            void start_streaming(UserStreamInterface * user_interface);
            void stop_streaming();
        };

        uvc_context_t * context;
        uvc_device_t * device;
        std::unique_ptr<UserStreamInterface> user_streams[MAX_STREAMS];
        std::unique_ptr<StreamInterface> streams[MAX_STREAMS];

        std::string cameraName;
        CalibrationInfo calib;

        uvc_device_handle_t * first_handle;
    public:
        UVCCamera(uvc_context_t * context, uvc_device_t * device);
        ~UVCCamera();

        const char * GetCameraName() const override final { return cameraName.c_str(); }

        void EnableStream(int stream, int width, int height, int fps, int format) override final;
        bool IsStreamEnabled(int stream) const override final { return (bool)streams[stream]; }
        void StartStreaming() override final;
        void StopStreaming() override final;
        void WaitAllStreams() override final;

        const void * GetImagePixels(int stream) const override final { return user_streams[stream] ? user_streams[stream]->get_image() : nullptr; }
        float GetDepthScale() const override final { return calib.depth_scale; }

        rs_intrinsics GetStreamIntrinsics(int stream) const override final;
        rs_extrinsics GetStreamExtrinsics(int from, int to) const override final;

        virtual int GetStreamSubdeviceNumber(int stream) const = 0;
        virtual CalibrationInfo RetrieveCalibration(uvc_device_handle_t * handle) = 0;
        virtual void SetStreamIntent() = 0;
    };
    
} // end namespace rs
#endif

#endif
