CC       := cc
CFLAGS   := -Wall -Wextra -O2 -std=c11
INCLUDES := -Isrc/app -Isrc/protocol -Isrc/net -Isrc/core -Isrc/core/client -Isrc/core/server -Isrc/tun -Isrc/ui
LDFLAGS  := -pthread
TARGET   := tund-cli
EXEEXT   :=
SAN_FLAGS := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
RUN_PREFIX := sudo ./

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

PROTO_SRC := src/protocol/protocol.c
APP_SRC  := src/app/main.c src/app/cli.c src/app/log.c src/app/platform.c
WIN_RUNTIME_SRC := src/app/win_runtime.c
UI_SRC   := src/ui/tui.c src/ui/render.c src/ui/events.c
CLIENT_SRC := src/core/client/client.c src/core/client/peers.c src/core/client/register.c src/core/client/handlers.c src/core/client/log.c
SERVER_SRC := src/core/server/server.c src/core/server/peers.c src/core/server/handlers.c src/core/server/data.c src/core/server/keepalive.c src/core/server/threads.c src/core/server/log.c
ifneq ($(filter MINGW%,$(UNAME_S)),)
    APP_SRC += $(WIN_RUNTIME_SRC)
endif
SRCS     := $(APP_SRC) src/net/network.c $(SERVER_SRC) $(CLIENT_SRC) $(UI_SRC) $(PROTO_SRC) $(TUN_SRC)
HDRS     := src/app/tund.h src/app/cli.h src/app/log.h src/app/platform.h src/app/win_runtime.h src/protocol/protocol.h src/tun/tun.h src/tun/windows/internal.h src/net/network.h src/core/server/server.h src/core/server/internal.h src/core/client/client.h src/core/client/internal.h src/ui/tui.h src/ui/tui_internal.h
TEST_PROTOCOL_SRCS := tests/test_protocol.c
TEST_SERVER_PEERS_SRCS := tests/test_server_peers.c src/core/server/peers.c $(PROTO_SRC)
TEST_SERVER_DATA_SRCS := tests/test_server_data.c src/core/server/data.c src/core/server/peers.c $(PROTO_SRC)
TEST_CLIENT_PEERS_SRCS := tests/test_client_peers.c src/core/client/peers.c
TEST_CLIENT_HANDLERS_SRCS := tests/test_client_handlers.c src/core/client/handlers.c src/core/client/peers.c $(PROTO_SRC)
TOOL_SRCS := tools/peerforge/main.c tools/peerforge/options.c tools/peerforge/net.c tools/peerforge/client.c
MACOS_UNIVERSAL_TARGET = $(DIST)/tund-cli-darwin-universal

.PHONY: all clean install uninstall macos-universal windows test sanitize tools verify

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
#   Result:   dist/tund-cli.exe + dist/wintun.dll

CROSS_W64   := x86_64-w64-mingw32
DIST        := dist
TARGET_WCON := $(DIST)/tund-cli.exe
WIN_TUN_SRCS := src/tun/windows/tun.c src/tun/windows/config.c src/tun/windows/process.c src/tun/windows/wintun_loader.c
WIN_APP_SRC := src/app/main.c src/app/cli.c src/app/log.c src/app/platform.c $(WIN_RUNTIME_SRC)
WIN_SRCS    := $(WIN_APP_SRC) src/net/network.c $(SERVER_SRC) $(CLIENT_SRC) $(UI_SRC) $(PROTO_SRC) $(WIN_TUN_SRCS)
WIN_CFLAGS  := -Wall -Wextra -O2 -std=c11 -D_WIN32_WINNT=0x0601 $(INCLUDES)
WIN_LIBS    := -static-libgcc -static -lws2_32 -liphlpapi -lpthread -luser32 -ladvapi32 -lshell32

$(DIST):
	mkdir -p $(DIST)

$(TARGET_WCON): $(WIN_SRCS) | $(DIST)
	$(CROSS_W64)-gcc $(WIN_CFLAGS) -o $@ $(WIN_SRCS) $(WIN_LIBS)
	@echo "  ✓ Built $(TARGET_WCON)"

TEST_TARGET := $(DIST)/test_protocol$(EXEEXT)
TEST_SERVER_PEERS_TARGET := $(DIST)/test_server_peers$(EXEEXT)
TEST_SERVER_DATA_TARGET := $(DIST)/test_server_data$(EXEEXT)
TEST_CLIENT_PEERS_TARGET := $(DIST)/test_client_peers$(EXEEXT)
TEST_CLIENT_HANDLERS_TARGET := $(DIST)/test_client_handlers$(EXEEXT)
TEST_TARGETS := $(TEST_TARGET) $(TEST_SERVER_PEERS_TARGET) $(TEST_SERVER_DATA_TARGET) $(TEST_CLIENT_PEERS_TARGET) $(TEST_CLIENT_HANDLERS_TARGET)
SAN_TEST_TARGET := $(DIST)/test_protocol_sanitize$(EXEEXT)
SAN_SERVER_PEERS_TARGET := $(DIST)/test_server_peers_sanitize$(EXEEXT)
SAN_SERVER_DATA_TARGET := $(DIST)/test_server_data_sanitize$(EXEEXT)
SAN_CLIENT_PEERS_TARGET := $(DIST)/test_client_peers_sanitize$(EXEEXT)
SAN_CLIENT_HANDLERS_TARGET := $(DIST)/test_client_handlers_sanitize$(EXEEXT)
SAN_TEST_TARGETS := $(SAN_TEST_TARGET) $(SAN_SERVER_PEERS_TARGET) $(SAN_SERVER_DATA_TARGET) $(SAN_CLIENT_PEERS_TARGET) $(SAN_CLIENT_HANDLERS_TARGET)
TOOL_TARGET := $(DIST)/peerforge$(EXEEXT)

test: $(TEST_TARGETS)
	./$(TEST_TARGET)
	./$(TEST_SERVER_PEERS_TARGET)
	./$(TEST_SERVER_DATA_TARGET)
	./$(TEST_CLIENT_PEERS_TARGET)
	./$(TEST_CLIENT_HANDLERS_TARGET)

$(TEST_TARGET): $(TEST_PROTOCOL_SRCS) $(PROTO_SRC) src/protocol/protocol.h | $(DIST)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(TEST_PROTOCOL_SRCS) $(PROTO_SRC) $(LDFLAGS)

$(TEST_SERVER_PEERS_TARGET): $(TEST_SERVER_PEERS_SRCS) $(HDRS) | $(DIST)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(TEST_SERVER_PEERS_SRCS) $(LDFLAGS)

$(TEST_SERVER_DATA_TARGET): $(TEST_SERVER_DATA_SRCS) $(HDRS) | $(DIST)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(TEST_SERVER_DATA_SRCS) $(LDFLAGS)

$(TEST_CLIENT_PEERS_TARGET): $(TEST_CLIENT_PEERS_SRCS) $(HDRS) | $(DIST)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(TEST_CLIENT_PEERS_SRCS) $(LDFLAGS)

$(TEST_CLIENT_HANDLERS_TARGET): $(TEST_CLIENT_HANDLERS_SRCS) $(HDRS) | $(DIST)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(TEST_CLIENT_HANDLERS_SRCS) $(LDFLAGS)

sanitize: $(SAN_TEST_TARGETS)
	./$(SAN_TEST_TARGET)
	./$(SAN_SERVER_PEERS_TARGET)
	./$(SAN_SERVER_DATA_TARGET)
	./$(SAN_CLIENT_PEERS_TARGET)
	./$(SAN_CLIENT_HANDLERS_TARGET)

$(SAN_TEST_TARGET): $(TEST_PROTOCOL_SRCS) $(PROTO_SRC) src/protocol/protocol.h | $(DIST)
	$(CC) $(CFLAGS) $(INCLUDES) $(SAN_FLAGS) -o $@ $(TEST_PROTOCOL_SRCS) $(PROTO_SRC) $(LDFLAGS) $(SAN_FLAGS)

$(SAN_SERVER_PEERS_TARGET): $(TEST_SERVER_PEERS_SRCS) $(HDRS) | $(DIST)
	$(CC) $(CFLAGS) $(INCLUDES) $(SAN_FLAGS) -o $@ $(TEST_SERVER_PEERS_SRCS) $(LDFLAGS) $(SAN_FLAGS)

$(SAN_SERVER_DATA_TARGET): $(TEST_SERVER_DATA_SRCS) $(HDRS) | $(DIST)
	$(CC) $(CFLAGS) $(INCLUDES) $(SAN_FLAGS) -o $@ $(TEST_SERVER_DATA_SRCS) $(LDFLAGS) $(SAN_FLAGS)

$(SAN_CLIENT_PEERS_TARGET): $(TEST_CLIENT_PEERS_SRCS) $(HDRS) | $(DIST)
	$(CC) $(CFLAGS) $(INCLUDES) $(SAN_FLAGS) -o $@ $(TEST_CLIENT_PEERS_SRCS) $(LDFLAGS) $(SAN_FLAGS)

$(SAN_CLIENT_HANDLERS_TARGET): $(TEST_CLIENT_HANDLERS_SRCS) $(HDRS) | $(DIST)
	$(CC) $(CFLAGS) $(INCLUDES) $(SAN_FLAGS) -o $@ $(TEST_CLIENT_HANDLERS_SRCS) $(LDFLAGS) $(SAN_FLAGS)

tools: $(TOOL_TARGET)
	@echo "  Run with: ./$(TOOL_TARGET) -s 127.0.0.1 -k <key> -n 253"

$(TOOL_TARGET): $(TOOL_SRCS) $(PROTO_SRC) src/protocol/protocol.h | $(DIST)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(TOOL_SRCS) $(PROTO_SRC) $(LDFLAGS)

macos-universal: $(MACOS_UNIVERSAL_TARGET)

$(MACOS_UNIVERSAL_TARGET): $(SRCS) $(HDRS) | $(DIST)
ifeq ($(UNAME_S),Darwin)
	$(CC) $(CFLAGS) -arch arm64 -arch x86_64 $(INCLUDES) -o $@ $(SRCS) $(LDFLAGS)
	@echo "  ✓ Built $(MACOS_UNIVERSAL_TARGET)"
else
	@echo "  macos-universal requires macOS"
	@exit 1
endif

verify:
	$(MAKE) test
	$(MAKE) tools
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
	@echo "    dist/tund-cli.exe server -k <key>"
	@echo "    dist/tund-cli.exe client -s <ip> -k <key>"

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/$(TARGET)
	@echo "  ✓ Installed to /usr/local/bin/$(TARGET)"

uninstall:
	rm -f /usr/local/bin/$(TARGET)
	@echo "  ✓ Uninstalled from /usr/local/bin"

clean:
	rm -f $(TARGET) $(TARGET_WCON) $(TOOL_TARGET) $(MACOS_UNIVERSAL_TARGET)
	rm -rf $(DIST)/
	@echo "  ✓ Cleaned"
