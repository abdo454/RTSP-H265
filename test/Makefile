
SRCS = ../src/CRtspSession.cpp ../src/CStreamer.cpp ../src/JPEGSamples.cpp ../src/SimStreamer.cpp  ../src/AVC.cpp \
../src/AVC.h \
  ../src/RTPEnc.h \
 ../src/Utils.cpp \
  ../src/RTPEnc.cpp \
  ../src/Utils.h \
 ../src/Network.cpp \
 ../src/Network.h
 
run: *.cpp ../src/*
	#skill testerver
	g++ -Wall -o testserver -I ../src -I . *.cpp $(SRCS)
	#./testserver
