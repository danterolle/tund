.PHONY: gui-check

gui-check:
	@if ! command -v flutter >/dev/null 2>&1; then \
		echo "flutter not found; install Flutter to check the GUI." >&2; \
		exit 1; \
	fi
	cd gui && flutter pub get && flutter analyze && flutter test
