# Description

This repository introduces a compact library designed for streaming raw H265 video files via an RTSP server. It builds upon two key foundations:

1. MICRO-RTSP Library: Originally limited to mjpeg formats, this library serves as a base.
2. RTP-H265-H264 Repository: An earlier project (available at [RTP-H265-H264](https://github.com/abdo454/RTP-H265-H264)) focused on streaming unencapsulated H.264/AVC and HEVC/H.265 files over RTP protocols.

# Usage posix/linux 

1.  To view the video independently, use ffplay:
    - Command: $ `ffplay sample_960x540.hevc`

2.  Navigate to the 'main' directory, execute make, and then run the generated ./testserver:

    1. This action initiates the RTSP server, which then awaits client connections.
    2. To stream the video, use the following command:
       1. over UDP: `ffplay rtsp://127.0.0.1:554/live/1`
       2. over TCP :`ffplay -rtsp_transport tcp  rtsp://127.0.0.1:554/live/1`
### References:

1. [Summary of streaming media transmission protocols - NALU, RTP, RTSP, RTMP, SDP, etc.](https://blog.csdn.net/qq_41205665/article/details/130749013).

# License

Copyright 2018 S. Abdo Daood, abdo.daood94@gmail.com (MIT License)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
