.DEFAULT_GOAL := all

include mk/config.mk
include mk/sources.mk
include mk/tests.mk
include mk/tools.mk
include mk/release.mk
include mk/lint.mk
include mk/format.mk

.PHONY: all clean install uninstall verify

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(SRCS) $(LDFLAGS)
	@echo ""
	@echo "  ✓ Built $(TARGET) for $(UNAME_S)"
	@echo "  Run with: $(RUN_PREFIX)$(TARGET) server --key-file <key-file>"
	@echo "         or: $(RUN_PREFIX)$(TARGET) client -s <server_ip> --key-file <key-file>"
	@echo ""

$(DIST):
	mkdir -p $(DIST)

verify:
	$(MAKE) format-check
	$(MAKE) lint
	$(MAKE) test
	$(MAKE) tools
	$(MAKE) peerforge-check
	$(MAKE) peerforge-wrong-key-check
	$(MAKE) sanitize
	$(MAKE) all
	$(MAKE) windows

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
