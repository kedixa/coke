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

#include "coke/go.h"

#include "workflow/WFGlobal.h"

namespace coke::detail {

ExecQueue *get_exec_queue(const std::string &name) {
    return WFGlobal::get_exec_queue(name);
}

Executor *get_compute_executor() {
    return WFGlobal::get_compute_executor();
}

SubTask *GoTaskBase::done() {
    SeriesWork *series = series_of(this);

    awaiter->done();

    delete this;
    return series->pop();
}

} // namespace coke::detail
