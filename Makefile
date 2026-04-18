CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -pedantic
SOURCES := main.cpp orbits.cpp waves.cpp starfield.cpp rain.cpp
ANIMATION_HEADERS := animations.h canvas.h standalone.h

ifeq ($(OS),Windows_NT)
EXEEXT := .exe
RUN_PREFIX := .\\
RM := del /Q
NULL_DEVICE := NUL
else
EXEEXT :=
RUN_PREFIX := ./
RM := rm -f
NULL_DEVICE := /dev/null
endif

TARGET := anim$(EXEEXT)
STANDALONE_TARGETS := orbits$(EXEEXT) waves$(EXEEXT) starfield$(EXEEXT) rain$(EXEEXT)

.PHONY: all run clean

ifeq ($(OS),Windows_NT)
.PHONY: anim orbits waves starfield rain

anim: $(TARGET)
orbits: orbits$(EXEEXT)
waves: waves$(EXEEXT)
starfield: starfield$(EXEEXT)
rain: rain$(EXEEXT)
endif

all: $(TARGET)

$(TARGET): $(SOURCES) $(ANIMATION_HEADERS)
	$(CXX) $(CXXFLAGS) -DANIMATION_LIBRARY $(SOURCES) -o $@

orbits$(EXEEXT): orbits.cpp $(ANIMATION_HEADERS)
	$(CXX) $(CXXFLAGS) $< -o $@

waves$(EXEEXT): waves.cpp $(ANIMATION_HEADERS)
	$(CXX) $(CXXFLAGS) $< -o $@

starfield$(EXEEXT): starfield.cpp $(ANIMATION_HEADERS)
	$(CXX) $(CXXFLAGS) $< -o $@

rain$(EXEEXT): rain.cpp $(ANIMATION_HEADERS)
	$(CXX) $(CXXFLAGS) $< -o $@

run: $(TARGET)
	$(RUN_PREFIX)$(TARGET)

clean:
	-$(RM) $(TARGET) $(STANDALONE_TARGETS) 2>$(NULL_DEVICE)
