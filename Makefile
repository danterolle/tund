CC       := cc
CFLAGS   := -Wall -Wextra -O2 -std=c11
INCLUDES := -Isrc/app -Isrc/protocol -Isrc/net -Isrc/core -Isrc/tun -Isrc/ui
LDFLAGS  := -pthread
TARGET   := tund
EXEEXT   :=
SAN_FLAGS := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
RUN_PREFIX := sudo ./

UNAME_S  := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
    TUN_SRC  := src/tun/linux.c
    CFLAGS   += -D_GNU_SOURCE
endif
ifeq ($(UNAME_S),Darwin)
    TUN_SRC  := src/tun/darwin.c
    CFLAGS   += -D_DARWIN_C_SOURCE
endif
ifneq ($(filter MINGW%,$(UNAME_S)),)
    TUN_SRC  := src/tun/windows.c src/tun/windows_config.c src/tun/windows_process.c src/tun/windows_wintun.c
    TARGET   := tund.exe
    EXEEXT   := .exe
    RUN_PREFIX :=
    LDFLAGS  := -static-libgcc -static -lws2_32 -liphlpapi -lpthread -luser32 -ladvapi32 -lshell32
endif

PROTO_SRC := src/protocol/protocol.c
APP_SRC  := src/app/main.c src/app/log.c
UI_SRC   := src/ui/tui.c src/ui/render.c src/ui/events.c
SRCS     := $(APP_SRC) src/net/network.c src/core/server.c src/core/client.c $(UI_SRC) $(PROTO_SRC) $(TUN_SRC)
HDRS     := src/app/tund.h src/app/log.h src/protocol/protocol.h src/tun/tun.h src/tun/windows_internal.h src/net/network.h src/core/server.h src/core/client.h src/ui/tui.h src/ui/tui_internal.h
TEST_SRCS := tests/test_protocol.c

.PHONY: all clean install uninstall windows test sanitize verify

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(SRCS) $(LDFLAGS)
	@echo ""
	@echo "  ✓ Built $(TARGET) for $(UNAME_S)"
	@echo "  Run with: $(RUN_PREFIX)$(TARGET) server"
	@echo "         or: $(RUN_PREFIX)$(TARGET) client -s <server_ip>"
	@echo ""

# Windows cross-compilation via mingw-w64
#   Requires: brew install mingw-w64
#   Build:    make windows
#   Result:   dist/tund.exe + dist/wintun.dll

CROSS_W64   := x86_64-w64-mingw32
DIST        := dist
TARGET_WCON := $(DIST)/tund.exe
WIN_TUN_SRCS := src/tun/windows.c src/tun/windows_config.c src/tun/windows_process.c src/tun/windows_wintun.c
WIN_SRCS    := $(APP_SRC) src/net/network.c src/core/server.c src/core/client.c $(UI_SRC) $(PROTO_SRC) $(WIN_TUN_SRCS)
WIN_CFLAGS  := -Wall -Wextra -O2 -std=c11 -D_WIN32_WINNT=0x0601 $(INCLUDES)
WIN_LIBS    := -static-libgcc -static -lws2_32 -liphlpapi -lpthread -luser32 -ladvapi32 -lshell32

$(DIST):
	mkdir -p $(DIST)

$(TARGET_WCON): $(WIN_SRCS) | $(DIST)
	$(CROSS_W64)-gcc $(WIN_CFLAGS) -o $@ $(WIN_SRCS) $(WIN_LIBS)
	@echo "  ✓ Built $(TARGET_WCON)"

TEST_TARGET := $(DIST)/test_protocol$(EXEEXT)
SAN_TEST_TARGET := $(DIST)/test_protocol_sanitize$(EXEEXT)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_SRCS) $(PROTO_SRC) src/protocol/protocol.h | $(DIST)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(TEST_SRCS) $(PROTO_SRC) $(LDFLAGS)

sanitize: $(SAN_TEST_TARGET)
	./$(SAN_TEST_TARGET)

$(SAN_TEST_TARGET): $(TEST_SRCS) $(PROTO_SRC) src/protocol/protocol.h | $(DIST)
	$(CC) $(CFLAGS) $(INCLUDES) $(SAN_FLAGS) -o $@ $(TEST_SRCS) $(PROTO_SRC) $(LDFLAGS) $(SAN_FLAGS)

verify:
	$(MAKE) test
	$(MAKE) sanitize
	$(MAKE) all
	$(MAKE) windows

WINTUN_URL    := https://www.wintun.net/builds/wintun-0.14.1.zip
WINTUN_SHA256 := 07c256185d6ee3652e09fa55c0b673e2624b565e02c4b9091c79ca7d2f24ef51

$(DIST)/wintun.dll: | $(DIST)
	@echo "  ↓ Downloading wintun.dll from wintun.net..."
	curl -fsSL $(WINTUN_URL) -o /tmp/wintun.zip
	printf '%s  %s\n' "$(WINTUN_SHA256)" "/tmp/wintun.zip" | shasum -a 256 -c -
	unzip -o /tmp/wintun.zip wintun/bin/amd64/wintun.dll -d /tmp >/dev/null
	cp /tmp/wintun/bin/amd64/wintun.dll $(DIST)/wintun.dll
	@echo "  ✓ wintun.dll downloaded"

windows: $(TARGET_WCON) $(DIST)/wintun.dll
	@echo ""
	@echo "  ✓ Deployed to dist/"
	@echo "  Run on Windows (UAC prompt if needed):"
	@echo "    dist/tund.exe server -k <key>"
	@echo "    dist/tund.exe client -s <ip> -k <key>"

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/$(TARGET)
	@echo "  ✓ Installed to /usr/local/bin/$(TARGET)"

uninstall:
	rm -f /usr/local/bin/$(TARGET)
	@echo "  ✓ Uninstalled from /usr/local/bin"

clean:
	rm -f $(TARGET) $(TARGET_WCON)
	rm -rf $(DIST)/
	@echo "  ✓ Cleaned"
