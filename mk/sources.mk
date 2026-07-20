PROTO_SRC := src/protocol/protocol.c
APP_SRC  := src/app/main.c src/app/cli.c src/app/log.c src/app/platform.c
WIN_RUNTIME_SRC := src/app/win_runtime.c
UI_SRC   := src/ui/tui.c src/ui/render.c src/ui/events.c
CLIENT_SRC := src/core/client/client.c src/core/client/peers.c src/core/client/register.c src/core/client/handlers.c src/core/client/session.c src/core/client/log.c
SERVER_SRC := src/core/server/server.c src/core/server/peers.c src/core/server/handlers.c src/core/server/data.c src/core/server/keepalive.c src/core/server/threads.c src/core/server/log.c

ifneq ($(filter MINGW%,$(UNAME_S)),)
    APP_SRC += $(WIN_RUNTIME_SRC)
endif

SRCS := $(APP_SRC) src/net/network.c $(SERVER_SRC) $(CLIENT_SRC) $(UI_SRC) $(PROTO_SRC) $(TUN_SRC)
HDRS := src/app/tund.h src/app/cli.h src/app/log.h src/app/platform.h src/app/win_runtime.h src/protocol/protocol.h src/tun/tun.h src/tun/windows/internal.h src/net/network.h src/core/server/server.h src/core/server/internal.h src/core/client/client.h src/core/client/internal.h src/ui/tui.h src/ui/tui_internal.h
