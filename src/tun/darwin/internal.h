#ifndef TUND_DARWIN_INTERNAL_H
#define TUND_DARWIN_INTERNAL_H

#ifdef __APPLE__

#include "tun.h"
#include "tund.h"

#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/wait.h>
#include <net/if.h>

#endif /* __APPLE__ */
#endif /* TUND_DARWIN_INTERNAL_H */
