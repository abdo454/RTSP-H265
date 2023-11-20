# Testserver

This project is to Streaming naked H265 file over RTSP server.

# Usage
1. Play the video use:
   run $ ffplay sample_960x540.hevc
2. Start Streaming
Run "make" to build and run the server.
Run ffplay rtsp://127.0.0.1:8554/live/1 start streaming
you can run the nakwed
that talks to that server.  If all is working you should see a static image
of my office that I captured using a ESP32-CAM.
