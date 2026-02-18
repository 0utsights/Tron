CXX      = g++
CXXFLAGS = -O2 -std=c++17 -Wall -Wextra
LDFLAGS  = -lncursesw
TARGET   = tron
SRCS     = main.cpp menu.cpp game.cpp config.cpp
OBJS     = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.PHONY: all clean install
