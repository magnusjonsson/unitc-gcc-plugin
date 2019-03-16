HOST_GCC=g++
TARGET_GCC=gcc
PLUGIN_SOURCE_FILES= unitc.cc
PLUGIN_SO_NAME= unitc.so
TEST_SOURCE_FILES= testsource.c

GCCPLUGINS_DIR:= $(shell $(TARGET_GCC) -print-file-name=plugin)
CXXFLAGS+= -I$(GCCPLUGINS_DIR)/include -fPIC -fno-rtti -fvisibility=hidden -O2 -Wall -Wextra -Werror

.PHONY: all test

all: ${PLUGIN_SO_NAME} test

clean:
	rm -f ${PLUGIN_SO_NAME}
	rm -f test_output_actual

${PLUGIN_SO_NAME}: $(PLUGIN_SOURCE_FILES)
	@$(HOST_GCC) -shared $(CXXFLAGS) $^ -o $@

test_output_actual: ${PLUGIN_SO_NAME} ${TEST_SOURCE_FILES}
	@${TARGET_GCC} -c -fplugin=${PWD}/${PLUGIN_SO_NAME} ${TEST_SOURCE_FILES} -o /dev/null 2> $@ | true

test: test_output_expected test_output_actual
	@diff -U2 $^
	@echo "OK!"

