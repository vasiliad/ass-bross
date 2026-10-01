# NanoWeb — сборка на Linux (FASM для Linux собирает Windows PE64 так же, как под Windows).
# Каталог INCLUDE берётся из Windows-пакета FASM (имена файлов в нижнем регистре).

FASM         ?= fasm
FASM_INCLUDE ?= $(HOME)/.local/opt/fasm/include
PYTHON       ?= .venv/bin/python

SRC     := $(shell find src -name '*.asm' -o -name '*.inc' -o -name '*.manifest')
GUI_EXE := build/nanoweb.exe
CON_EXE := build/nanoweb-con.exe

.PHONY: all check test clean

all: $(GUI_EXE) $(CON_EXE)

build:
	mkdir -p build

$(GUI_EXE): $(SRC) tools/patch_pe.py | build
	cd src && INCLUDE=$(FASM_INCLUDE) $(FASM) -d CONSOLE=0 main.asm ../$@
	$(PYTHON) tools/patch_pe.py $@

$(CON_EXE): $(SRC) tools/patch_pe.py | build
	cd src && INCLUDE=$(FASM_INCLUDE) $(FASM) -d CONSOLE=1 main.asm ../$@
	$(PYTHON) tools/patch_pe.py $@

check: all
	$(PYTHON) tools/check_pe.py $(GUI_EXE) $(CON_EXE)

test: check
	$(PYTHON) -m pytest -q tests

clean:
	rm -rf build
