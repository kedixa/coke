#ifndef COKE_COKE_H
#define COKE_COKE_H

#include <functional>
#include <concepts>
#include <memory>

/**
 * coke.h includes most of the headers, except for network-related parts
*/

#include "coke/global.h"
#include "coke/fileio.h"
#include "coke/go.h"
#include "coke/latch.h"
#include "coke/sleep.h"
#include "coke/qps_pool.h"
#include "coke/wait.h"
#include "coke/generic_awaiter.h"
#include "coke/series.h"
#include "coke/semaphore.h"
#include "coke/mutex.h"
#include "coke/shared_mutex.h"
#include "coke/future.h"
#include "coke/make_task.h"

#endif // COKE_COKE_H
