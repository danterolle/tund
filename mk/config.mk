CC       := cc
AR       := ar
CFLAGS   := -Wall -Wextra -O2 -std=c11
INCLUDES := -Isrc/app -Isrc/protocol -Isrc/net -Isrc/core -Isrc/core/client -Isrc/core/server -Isrc/tun -Isrc/ui -Ithird_party/monocypher
TEST_INCLUDES := $(INCLUDES) -Itools/sitest -Itests/support
LDFLAGS  := -pthread
TARGET   := tund-cli
EXEEXT   :=
SAN_FLAGS := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
RUN_PREFIX := sudo ./
DIST     := dist

UNAME_S  := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
    TUN_SRC  := src/tun/linux/tun.c
    CFLAGS   += -D_GNU_SOURCE
endif
ifeq ($(UNAME_S),Darwin)
    TUN_SRC  := src/tun/darwin/tun.c src/tun/darwin/config.c
    CFLAGS   += -D_DARWIN_C_SOURCE
endif
ifneq ($(filter MINGW%,$(UNAME_S)),)
    TUN_SRC  := src/tun/windows/tun.c src/tun/windows/config.c src/tun/windows/process.c src/tun/windows/wintun_loader.c
    TARGET   := tund-cli.exe
    EXEEXT   := .exe
    RUN_PREFIX :=
    LDFLAGS  := -static-libgcc -static -lws2_32 -liphlpapi -lpthread -luser32 -ladvapi32 -lshell32
endif
