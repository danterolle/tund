CC       := cc
CFLAGS   := -Wall -Wextra -O2 -std=c11
LDFLAGS  := -pthread
TARGET   := tund
EXEEXT   :=

UNAME_S  := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
    TUN_SRC  := src/tun_linux.c
    CFLAGS   += -D_GNU_SOURCE
endif
ifeq ($(UNAME_S),Darwin)
    TUN_SRC  := src/tun_darwin.c
    CFLAGS   += -D_DARWIN_C_SOURCE
endif
ifneq ($(filter MINGW%,$(UNAME_S)),)
    TUN_SRC  := src/tun_windows.c
    TARGET   := tund.exe
    EXEEXT   := .exe
    LDFLAGS  := -static-libgcc -static -lws2_32 -liphlpapi -lpthread -luser32 -ladvapi32
endif

SRCS     := src/main.c src/network.c src/server.c src/client.c src/tui.c $(TUN_SRC)
HDRS     := src/tund.h src/protocol.h src/tun.h src/network.h src/server.h src/client.h src/tui.h
TEST_SRCS := tests/test_protocol.c

.PHONY: all clean install uninstall windows test

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)
	@echo ""
	@echo "  ✓ Built $(TARGET) for $(UNAME_S)"
	@echo "  Run with: sudo ./$(TARGET) server"
	@echo "         or: sudo ./$(TARGET) client -s <server_ip>"
	@echo ""

# Windows cross-compilation via mingw-w64
#   Requires: brew install mingw-w64
#   Build:    make windows
#   Result:   dist/tund.exe + dist/wintun.dll

CROSS_W64   := x86_64-w64-mingw32
DIST        := dist
TARGET_WCON := $(DIST)/tund.exe
WIN_SRCS    := src/main.c src/network.c src/server.c src/client.c src/tui.c src/tun_windows.c
WIN_CFLAGS  := -Wall -Wextra -O2 -std=c11 -D_WIN32_WINNT=0x0601
WIN_LIBS    := -static-libgcc -static -lws2_32 -liphlpapi -lpthread -luser32 -ladvapi32

$(DIST):
	mkdir -p $(DIST)

$(TARGET_WCON): $(WIN_SRCS) | $(DIST)
	$(CROSS_W64)-gcc $(WIN_CFLAGS) -o $@ $(WIN_SRCS) $(WIN_LIBS)
	@echo "  ✓ Built $(TARGET_WCON)"

TEST_TARGET := $(DIST)/test_protocol$(EXEEXT)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_SRCS) src/protocol.h | $(DIST)
	$(CC) $(CFLAGS) -I. -o $@ $(TEST_SRCS) $(LDFLAGS)

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
	@echo "  Run as Administrator on Windows:"
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
