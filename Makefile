CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -pedantic
SOURCES := main.cpp orbits.cpp waves.cpp starfield.cpp rain.cpp
ANIMATION_HEADERS := animations.h canvas.h standalone.h

.PHONY: all run clean

all: anim

anim: $(SOURCES) $(ANIMATION_HEADERS)
	$(CXX) $(CXXFLAGS) -DANIMATION_LIBRARY $(SOURCES) -o $@

orbits: orbits.cpp $(ANIMATION_HEADERS)
	$(CXX) $(CXXFLAGS) $< -o $@

waves: waves.cpp $(ANIMATION_HEADERS)
	$(CXX) $(CXXFLAGS) $< -o $@

starfield: starfield.cpp $(ANIMATION_HEADERS)
	$(CXX) $(CXXFLAGS) $< -o $@

rain: rain.cpp $(ANIMATION_HEADERS)
	$(CXX) $(CXXFLAGS) $< -o $@

run: anim
	./anim

clean:
	rm -f anim orbits waves starfield rain
