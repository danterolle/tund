.PHONY: macos-universal windows

# Windows cross-compilation via mingw-w64
#   Requires: brew install mingw-w64
#   Build:    make windows
#   Result:   dist/tund-cli.exe + dist/wintun.dll

CROSS_W64   := x86_64-w64-mingw32
TARGET_WCON := $(DIST)/tund-cli.exe
WIN_TUN_SRCS := src/tun/windows/tun.c src/tun/windows/config.c src/tun/windows/process.c src/tun/windows/wintun_loader.c
WIN_APP_SRC := src/app/main.c src/app/cli.c src/app/log.c src/app/platform.c $(WIN_RUNTIME_SRC)
WIN_SRCS    := $(WIN_APP_SRC) src/net/network.c $(SERVER_SRC) $(CLIENT_SRC) $(UI_SRC) $(PROTO_SRC) $(WIN_TUN_SRCS)
WIN_CFLAGS  := -Wall -Wextra -O2 -std=c11 -D_WIN32_WINNT=0x0601 $(INCLUDES)
WIN_LIBS    := -static-libgcc -static -lws2_32 -liphlpapi -lpthread -luser32 -ladvapi32 -lshell32

MACOS_UNIVERSAL_TARGET = $(DIST)/tund-cli-darwin-universal
WINTUN_URL    := https://www.wintun.net/builds/wintun-0.14.1.zip
WINTUN_SHA256 := 07c256185d6ee3652e09fa55c0b673e2624b565e02c4b9091c79ca7d2f24ef51

$(TARGET_WCON): $(WIN_SRCS) | $(DIST)
	$(CROSS_W64)-gcc $(WIN_CFLAGS) -o $@ $(WIN_SRCS) $(WIN_LIBS)
	@echo "  ✓ Built $(TARGET_WCON)"

macos-universal: $(MACOS_UNIVERSAL_TARGET)

$(MACOS_UNIVERSAL_TARGET): $(SRCS) $(HDRS) | $(DIST)
ifeq ($(UNAME_S),Darwin)
	$(CC) $(CFLAGS) -arch arm64 -arch x86_64 $(INCLUDES) -o $@ $(SRCS) $(LDFLAGS)
	@echo "  ✓ Built $(MACOS_UNIVERSAL_TARGET)"
else
	@echo "  macos-universal requires macOS"
	@exit 1
endif

$(DIST)/wintun.dll: | $(DIST)
	tools/fetch-wintun.sh "$(WINTUN_URL)" "$(WINTUN_SHA256)" "$@"

windows: $(TARGET_WCON) $(DIST)/wintun.dll
	@echo ""
	@echo "  ✓ Deployed to dist/"
	@echo "  Run on Windows (UAC prompt if needed):"
	@echo "    dist/tund-cli.exe server -k <key>"
	@echo "    dist/tund-cli.exe client -s <ip> -k <key>"
