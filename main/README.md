# TestServer

## Overview

TestServer is a project designed for streaming raw H.265 files over an RTSP server. It leverages an RTSP server library that utilizes RTP sockets for streaming, opening RTCP sockets without utilizing them to receive control commands from clients.

## Usage Instructions

### Playing the Video

To play the video, execute the following command:

```sh
ffplay sample_960x540.hevc
```

### Starting the Stream

To build and run the server for streaming:

1. Execute `make` to compile the project, this run `./testserver` to initiate the server.
2. Start streaming by running the command:
    1. over UDP: `ffplay rtsp://127.0.0.1:554/live/1`
    2. over TCP :`ffplay -rtsp_transport tcp  rtsp://127.0.0.1:554/live/1`

### Additional Information

- **RTSP Server Library:** The library employed by this project opens RTCP sockets but does not use them to receive control commands from clients. This approach focuses on the RTP socket for streaming functionality.