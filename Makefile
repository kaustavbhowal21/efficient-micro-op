CXX = g++
CXXFLAGS = -std=c++20 -O3 -Wall -Wextra -Iinclude
SRCS = src/main.cc src/uop_cache.cc src/jenks.cc src/profiler.cc src/trace_loader.cc
TARGET = uop_sim.exe

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o

