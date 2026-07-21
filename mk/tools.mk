.PHONY: tools peerforge-check peerforge-wrong-key-check

TOOL_TARGET := $(DIST)/peerforge$(EXEEXT)
PEERFORGE_SERVER_TARGET := $(DIST)/peerforge-server$(EXEEXT)
PEERFORGE_CHECK_KEY := peerforge-ci-key
PEERFORGE_CHECK_PORT ?= 19909
PEERFORGE_WRONG_KEY_PORT ?= 19910
PEERFORGE_WRONG_KEY := peerforge-wrong-key

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
	tools/check-peerforge.sh "./$(TOOL_TARGET)" "./$(PEERFORGE_SERVER_TARGET)" "$(PEERFORGE_CHECK_PORT)" "$(PEERFORGE_CHECK_KEY)" "$(DIST)/peerforge-server.log"

peerforge-wrong-key-check: $(TOOL_TARGET) $(PEERFORGE_SERVER_TARGET)
	tools/check-peerforge-wrong-key.sh "./$(TOOL_TARGET)" "./$(PEERFORGE_SERVER_TARGET)" "$(PEERFORGE_WRONG_KEY_PORT)" "$(PEERFORGE_CHECK_KEY)" "$(PEERFORGE_WRONG_KEY)" "$(DIST)/peerforge-wrong-key-server.log"
