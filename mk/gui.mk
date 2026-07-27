.PHONY: gui-check gui-clean

GUI_GENERATED_DIRS := \
	gui/.dart_tool \
	gui/build \
	gui/linux/flutter/ephemeral \
	gui/macos/Flutter/ephemeral \
	gui/windows/flutter/ephemeral

gui-clean:
	rm -rf $(GUI_GENERATED_DIRS)

gui-check:
	@if ! command -v flutter >/dev/null 2>&1; then \
		echo "flutter not found; install Flutter to check the GUI." >&2; \
		exit 1; \
	fi
	cd gui && flutter pub get && flutter analyze && flutter test
