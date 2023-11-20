# Description

This is a small Library for Streaming naked H265 video file Over RTSP server. It's based on :

1. the Library MICRO-RTSP which are limited for mjpeg formats.
2. the previous repository [RTP-H265-H264](https://github.com/abdo454/RTP-H265-H264) which for Streaming naked H.264/AVC & HEVC/H.265 files over RTP protocols.

# posix/linux usage

There is a small standalone example [here](/test/RTSPTestServer.cpp). You can build it by following [these](/test/README.md) directions. The usage of the two key classes (CRtspSession and SimStreamer) are very similar to to the ESP32 usage.

# License

Copyright 2018 S. Kevin Hester-Chow, kevinh@geeksville.com (MIT License)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
