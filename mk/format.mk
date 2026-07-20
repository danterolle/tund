.PHONY: format format-check format-c format-c-check

format:
	tools/format.sh

format-check:
	tools/format.sh --check

format-c: format

format-c-check: format-check
