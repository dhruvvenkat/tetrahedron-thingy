CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -pedantic

.PHONY: all run clean

all: anim

anim: main.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

run: anim
	./anim

clean:
	rm -f anim
