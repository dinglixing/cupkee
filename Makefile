##
## MIT License
##
## This file is part of cupkee project.
##
## Copyright (c) 2016 Lixing Ding <ding.lixing@gmail.com>
##
## Permission is hereby granted, free of charge, to any person obtaining a copy
## of this software and associated documentation files (the "Software"), to deal
## in the Software without restriction, including without limitation the rights
## to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
## copies of the Software, and to permit persons to whom the Software is
## furnished to do so, subject to the following conditions:
##
## The above copyright notice and this permission notice shall be included in all
## copies or substantial portions of the Software.
##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
## AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
## LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
## OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
## SOFTWARE.
##

-include board.mk

ifeq (${CPU},)
$(info "Target processor not specified...")
endif

ifeq (${BSP},)
$(info "Target bsp not specified...")
endif

# Define default target processor
export CPU ?= test


ifeq (${BASE_DIR},)
BASE_DIR = ${PWD}
else
MAIN_DIR = ${PWD}
endif

export BSP
export BASE_DIR
export MAIN_DIR
export MAKE_DIR = ${BASE_DIR}/make

export INC_DIR = ${BASE_DIR}/include
export BSP_DIR = ${BASE_DIR}/bsp
export UTL_DIR = ${BASE_DIR}/utils
export SYS_DIR = ${BASE_DIR}/system
export TST_DIR = ${BASE_DIR}/test
export FRAMEWORK_DIR = ${BASE_DIR}/frameworks

export SHARE_DIR = ${BASE_DIR}/share
export LANG_DIR  = ${SHARE_DIR}/panda

ifeq (${MAIN_DIR},)
BUILD_DIR = ${BASE_DIR}/build/${CPU}
else
BUILD_DIR = ${MAIN_DIR}/build/${CPU}
endif

export BUILD_DIR
export BSP_BUILD_DIR = ${BUILD_DIR}/bsp
export UTL_BUILD_DIR = ${BUILD_DIR}/utils
export SYS_BUILD_DIR = ${BUILD_DIR}/sys
export MOD_BUILD_DIR = ${BUILD_DIR}/modules
export LANG_BUILD_DIR = ${BUILD_DIR}/lang
export BOOT_ADDR

ifeq (${FRAMEWORK},atom)
all: atom
	@printf "OK\n"
else
ifeq (${FRAMEWORK},ogin)
all: ogin
	@printf "OK\n"
else
all: test
	@printf "ok\n"
endif
endif

setup:
	git pull
	git submodule init
	git submodule update
	make -C share/libopencm3

build:
	@mkdir -p ${LANG_BUILD_DIR} ${BSP_BUILD_DIR} ${UTL_BUILD_DIR} ${SYS_BUILD_DIR}

bsp:
	@make -C ${BSP_BUILD_DIR} -f ${MAKE_DIR}/bsp.mk

utils:
	@make -C ${UTL_BUILD_DIR} -f ${MAKE_DIR}/utils.mk

sys:
	@make -C ${SYS_BUILD_DIR} -f ${MAKE_DIR}/sys.mk

lang:
	@make -C ${LANG_BUILD_DIR} -f ${MAKE_DIR}/lang.mk

module: build bsp sys utils lang
	@mkdir -p ${BUILD_DIR}/lib
	@make -C ${BUILD_DIR} -f ${MAKE_DIR}/module.mk extend

loader: build bsp sys utils
	@make -C ${BUILD_DIR} -f ${MAKE_DIR}/loader.mk extend

ogin: build bsp sys utils
	@make -C ${BUILD_DIR} -f ${MAKE_DIR}/ogin.mk extend

atom: build bsp sys utils lang
	@make -C ${BUILD_DIR} -f ${MAKE_DIR}/atom.mk extend

test: build sys utils lang
	@rm -rf ${BUILD_DIR}/test.elf
	@make -C ${BUILD_DIR} -f ${MAKE_DIR}/test.mk
	${BUILD_DIR}/test.elf

clean:
	@rm -rf ${BUILD_DIR}

.PHONY: clean build main bsp utils lang sys ogin atom

