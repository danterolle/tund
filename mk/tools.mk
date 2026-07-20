.PHONY: tools peerforge-check

TOOL_TARGET := $(DIST)/peerforge$(EXEEXT)
PEERFORGE_SERVER_TARGET := $(DIST)/peerforge-server$(EXEEXT)
PEERFORGE_CHECK_KEY := peerforge-ci-key
PEERFORGE_CHECK_PORT ?= 19909

TOOL_SRCS := tools/peerforge/main.c tools/peerforge/options.c tools/peerforge/net.c tools/peerforge/client.c
PEERFORGE_SERVER_SRCS := tools/peerforge/server.c src/net/network.c src/core/server/handlers.c src/core/server/peers.c src/core/server/data.c src/core/server/keepalive.c $(PROTO_SRC)

tools: $(TOOL_TARGET) $(PEERFORGE_SERVER_TARGET)
	@echo "  Run with: ./$(TOOL_TARGET) -s 127.0.0.1 -k <key> -n 253"
	@echo "  Run integration check with: make peerforge-check"

$(TOOL_TARGET): $(TOOL_SRCS) $(PROTO_SRC) src/protocol/protocol.h | $(DIST)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(TOOL_SRCS) $(PROTO_SRC) $(LDFLAGS)

$(PEERFORGE_SERVER_TARGET): $(PEERFORGE_SERVER_SRCS) $(HDRS) | $(DIST)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(PEERFORGE_SERVER_SRCS) $(LDFLAGS)

peerforge-check: $(TOOL_TARGET) $(PEERFORGE_SERVER_TARGET)
	@set -e; \
	log="$(DIST)/peerforge-server.log"; \
	"./$(PEERFORGE_SERVER_TARGET)" -p $(PEERFORGE_CHECK_PORT) -k "$(PEERFORGE_CHECK_KEY)" -t 20 >"$$log" 2>&1 & \
	server_pid=$$!; \
	cleanup() { status=$$?; kill $$server_pid >/dev/null 2>&1 || true; wait $$server_pid >/dev/null 2>&1 || true; if [ $$status -ne 0 ]; then cat "$$log" >&2 || true; fi; exit $$status; }; \
	trap cleanup EXIT INT TERM; \
	sleep 1; \
	"./$(TOOL_TARGET)" -s 127.0.0.1 -p $(PEERFORGE_CHECK_PORT) -k "$(PEERFORGE_CHECK_KEY)" -n 32 -t 3000 -K 2 -d 32
