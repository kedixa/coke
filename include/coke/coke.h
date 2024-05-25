/**
 * Copyright 2024 Coke Project (https://github.com/kedixa/coke)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Authors: kedixa (https://github.com/kedixa)
*/

#ifndef COKE_COKE_H
#define COKE_COKE_H

#include <functional>
#include <concepts>
#include <memory>

/**
 * coke.h includes most of the headers, except for network-related parts.
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
#include "coke/condition.h"
#include "coke/queue.h"

#endif // COKE_COKE_H
