# Copyright (c) 2026 Johannes Overmann
#
# Distributed under the Boost Software License, Version 1.0.

WARNING_FLAGS ?= -Weverything \
	-Wno-c++98-compat \
	-Wno-c++98-compat-pedantic \
	-Wno-padded \
	-Wno-shorten-64-to-32 \
	-Wno-missing-prototypes \
	-Wno-sign-conversion \
	-Wno-implicit-int-conversion \
	-Wno-poison-system-directories \
	-fcomment-block-commands=n \
	-Wno-covered-switch-default \
	-Wno-exit-time-destructors \
	-Wno-global-constructors \
	-Wno-unsafe-buffer-usage \
	-Wno-extra-semi-stmt
CPPFLAGS ?= -pedantic
CXXSTD ?= -std=c++20
CXX ?= g++
BUILD ?= release
CXXFLAGS_COMMON ?= -Wall
CXXFLAGS_DEBUG ?= -O0 -g
CXXFLAGS_RELEASE ?= -O3 -DNDEBUG

ifeq ($(BUILD),debug)
CXXFLAGS ?= $(CXXFLAGS_COMMON) $(CXXFLAGS_DEBUG)
else ifeq ($(BUILD),release)
CXXFLAGS ?= $(CXXFLAGS_COMMON) $(CXXFLAGS_RELEASE)
else
$(error Unknown BUILD='$(BUILD)', expected debug or release)
endif

BUILDDIR=build-$(BUILD)
UNIT_TEST_BUILDDIR=build-unit-test-$(BUILD)
SOURCES = $(wildcard src/*.cpp)
OBJECTS = $(SOURCES:%.cpp=$(BUILDDIR)/%.o)
DEPENDS := $(SOURCES:%.cpp=$(BUILDDIR)/%.d)
UNIT_TEST_OBJECTS = $(SOURCES:%.cpp=$(UNIT_TEST_BUILDDIR)/%.o)
UNIT_TEST_DEPENDS := $(SOURCES:%.cpp=$(UNIT_TEST_BUILDDIR)/%.d)

TARGET = fixtext
default: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $^ -o $@
	@echo "Done."

$(BUILDDIR)/%.o: %.cpp $(BUILDDIR)/%.d
	$(CXX) $(CXXSTD) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILDDIR)/%.d: %.cpp Makefile
	@mkdir -p $(@D)
	$(CXX) $(CXXSTD) $(CPPFLAGS) -MM -MQ $@ $< -o $@

clean:
	rm -rf build build-* build-unit-test $(TARGET) unit_test
	find . -name '*~' -delete

$(UNIT_TEST_BUILDDIR)/%.o: %.cpp $(UNIT_TEST_BUILDDIR)/%.d
	$(CXX) $(CXXSTD) $(CPPFLAGS) -D ENABLE_UNIT_TEST $(CXXFLAGS) -c $< -o $@

$(UNIT_TEST_BUILDDIR)/%.d: %.cpp Makefile
	@mkdir -p $(@D)
	$(CXX) $(CXXSTD) $(CPPFLAGS) -D ENABLE_UNIT_TEST -MM -MQ $@ $< -o $@

unit_test: $(UNIT_TEST_OBJECTS)
	$(CXX) $^ -o $@
	@echo "Done."
	./unit_test

test: unit_test $(TARGET)
	./fixtext -a testdata

format:
	clang-format -i --style=file src/*.hpp src/*.cpp

warnings:
	$(MAKE) clean
	$(MAKE) CXXFLAGS="$(CXXFLAGS_RELEASE) $(WARNING_FLAGS)" $(TARGET)

.PHONY: clean default unit_test test format warnings

ifeq ($(findstring $(MAKECMDGOALS),clean),)
ifneq ($(MAKECMDGOALS),unit_test)
-include $(DEPENDS)
endif
ifneq ($(filter unit_test,$(MAKECMDGOALS)),)
-include $(UNIT_TEST_DEPENDS)
endif
endif
