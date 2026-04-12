CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Isrc
LDLIBS   = -lcurl

SRCS = src/main.cpp \
       src/config.cpp \
       src/repl.cpp \
       src/json.cpp \
       src/agent/Loop.cpp \
       src/tools/Tool.cpp \
       src/tools/ReadFile.cpp \
       src/tools/WriteFile.cpp \
       src/tools/EditFile.cpp \
       src/tools/Bash.cpp \
       src/tools/Grep.cpp \
       src/tools/Glob.cpp \
       src/mcp/Client.cpp \
       src/mcp/Config.cpp \
       src/mcp/Transport.cpp \
       src/mcp/HttpTransport.cpp \
       src/mcp/StdioTransport.cpp \
       src/mcp/Cli.cpp \
       src/provider/Anthropic.cpp \
       src/provider/OpenAICompat.cpp

OBJS = $(SRCS:.cpp=.o)

pluto: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f pluto $(OBJS)

.PHONY: clean
