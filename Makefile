CXX      = g++
CXXFLAGS = -O2 -std=c++17 -Wall
LDFLAGS  = -lncursesw
TARGET   = tron
SRCS     = main.cpp menu.cpp game.cpp config.cpp
OBJS     = $(SRCS:.cpp=.o)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) /usr/local/bin/$(TARGET)

.PHONY: clean install
