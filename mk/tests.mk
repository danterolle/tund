.PHONY: test sanitize

SITEST_SRC := tools/sitest/sitest.c
SITEST_HDR := tools/sitest/sitest.h
TEST_SUPPORT_SRC := tests/support/test_support.c
TEST_SUPPORT_HDR := tests/support/test_support.h
TEST_SUPPORT_SRCS := $(TEST_SUPPORT_SRC) $(PROTO_SRC)

SITEST_LIB := $(DIST)/libsitest.a
TEST_SUPPORT_LIB := $(DIST)/libtest-support.a
SAN_SITEST_LIB := $(DIST)/libsitest_sanitize.a
SAN_TEST_SUPPORT_LIB := $(DIST)/libtest-support_sanitize.a

SITEST_OBJS := $(DIST)/test-obj/$(SITEST_SRC:.c=.o)
TEST_SUPPORT_OBJS := $(addprefix $(DIST)/test-obj/,$(TEST_SUPPORT_SRCS:.c=.o))
SAN_SITEST_OBJS := $(DIST)/test-obj-sanitize/$(SITEST_SRC:.c=.o)
SAN_TEST_SUPPORT_OBJS := $(addprefix $(DIST)/test-obj-sanitize/,$(TEST_SUPPORT_SRCS:.c=.o))

TEST_NAMES := \
	protocol \
	server_peers \
	server_data \
	server_handlers \
	client_peers \
	client_handlers

test_protocol_SRCS := tests/protocol/test_protocol.c $(PROTO_SRC)
test_protocol_LIBS := $(SITEST_LIB)
test_protocol_SAN_LIBS := $(SAN_SITEST_LIB)
test_protocol_DEPS := src/protocol/protocol.h $(SITEST_HDR)

test_server_peers_SRCS := tests/server/test_peers.c src/core/server/peers.c
test_server_data_SRCS := tests/server/test_data.c src/core/server/data.c src/core/server/peers.c
test_server_handlers_SRCS := tests/server/test_handlers.c src/app/events.c src/core/server/handlers.c src/core/server/peers.c src/core/server/data.c src/core/server/keepalive.c
test_client_peers_SRCS := tests/client/test_peers.c src/core/client/peers.c
test_client_handlers_SRCS := tests/client/test_handlers.c src/core/client/handlers.c src/core/client/peers.c src/core/client/session.c

TEST_TARGETS := $(addprefix $(DIST)/test_,$(addsuffix $(EXEEXT),$(TEST_NAMES)))
SAN_TEST_TARGETS := $(addprefix $(DIST)/test_,$(addsuffix _sanitize$(EXEEXT),$(TEST_NAMES)))

test: $(TEST_TARGETS)
	@set -e; for test_bin in $(TEST_TARGETS); do "./$$test_bin"; done

sanitize: $(SAN_TEST_TARGETS)
	@set -e; for test_bin in $(SAN_TEST_TARGETS); do "./$$test_bin"; done

$(DIST)/test-obj/%.o: %.c $(HDRS) $(SITEST_HDR) $(TEST_SUPPORT_HDR) | $(DIST)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(TEST_INCLUDES) -c -o $@ $<

$(DIST)/test-obj-sanitize/%.o: %.c $(HDRS) $(SITEST_HDR) $(TEST_SUPPORT_HDR) | $(DIST)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(TEST_INCLUDES) $(SAN_FLAGS) -c -o $@ $<

$(SITEST_LIB): $(SITEST_OBJS) | $(DIST)
	rm -f $@
	$(AR) rcs $@ $(SITEST_OBJS)

$(TEST_SUPPORT_LIB): $(TEST_SUPPORT_OBJS) | $(DIST)
	rm -f $@
	$(AR) rcs $@ $(TEST_SUPPORT_OBJS)

$(SAN_SITEST_LIB): $(SAN_SITEST_OBJS) | $(DIST)
	rm -f $@
	$(AR) rcs $@ $(SAN_SITEST_OBJS)

$(SAN_TEST_SUPPORT_LIB): $(SAN_TEST_SUPPORT_OBJS) | $(DIST)
	rm -f $@
	$(AR) rcs $@ $(SAN_TEST_SUPPORT_OBJS)

define test_rule
$(DIST)/test_$(1)$(EXEEXT): $$(test_$(1)_SRCS) $$(SITEST_LIB) $$(TEST_SUPPORT_LIB) $$(HDRS) $$(TEST_SUPPORT_HDR) $$(test_$(1)_DEPS) | $$(DIST)
	$$(CC) $$(CFLAGS) $$(TEST_INCLUDES) -o $$@ $$(test_$(1)_SRCS) $$(or $$(test_$(1)_LIBS),$$(SITEST_LIB) $$(TEST_SUPPORT_LIB)) $$(LDFLAGS)

$(DIST)/test_$(1)_sanitize$(EXEEXT): $$(test_$(1)_SRCS) $$(SAN_SITEST_LIB) $$(SAN_TEST_SUPPORT_LIB) $$(HDRS) $$(TEST_SUPPORT_HDR) $$(test_$(1)_DEPS) | $$(DIST)
	$$(CC) $$(CFLAGS) $$(TEST_INCLUDES) $$(SAN_FLAGS) -o $$@ $$(test_$(1)_SRCS) $$(or $$(test_$(1)_SAN_LIBS),$$(SAN_SITEST_LIB) $$(SAN_TEST_SUPPORT_LIB)) $$(LDFLAGS) $$(SAN_FLAGS)
endef

$(foreach test_name,$(TEST_NAMES),$(eval $(call test_rule,$(test_name))))
