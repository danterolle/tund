.PHONY: lint lint-c

LINT_TEST_SRCS := $(foreach test_name,$(TEST_NAMES),$(test_$(test_name)_SRCS))

LINT_C_SRCS := $(filter-out $(THIRD_PARTY_SRC),$(sort \
	$(SRCS) \
	$(LINT_TEST_SRCS) \
	$(SITEST_SRC) \
	$(TEST_SUPPORT_SRCS) \
	$(TOOL_SRCS) \
	$(PEERFORGE_SERVER_SRCS)))

lint:
	tools/lint.sh $(CFLAGS) $(TEST_INCLUDES) -- $(LINT_C_SRCS)

lint-c: lint
