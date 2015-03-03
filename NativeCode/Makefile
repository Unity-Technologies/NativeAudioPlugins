SRCS=AudioPluginUtil.cpp \
Plugin_ConvolutionReverb.cpp \
Plugin_CorrelationMeter.cpp \
Plugin_Equalizer.cpp \
Plugin_LevelMixer.cpp \
Plugin_Lofinator.cpp \
Plugin_LoudnessMeter.cpp \
Plugin_Multiband.cpp \
Plugin_NoiseBox.cpp \
Plugin_RingModulator.cpp \
Plugin_StereoWidener.cpp \
Plugin_TeeBee.cpp \
Plugin_TeeDee.cpp \
Plugin_Teleport.cpp \
Plugin_Vocoder.cpp
OBJS=$(SRCS:.cpp=*.o)
OUTPUT=libAudioPluginDemo.so
CXXFLAGS=-I. -O2 -fPIC
LDFLAGS=-shared -rdynamic -fPIC
CXX=g++

all: $(OUTPUT)
clean:
	rm -f $(OUTPUT) $(OBJS)

$(OUTPUT): $(OBJS)
	$(CXX) $(LDFLAGS) -o $(OUTPUT) $(OBJS)

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $<
