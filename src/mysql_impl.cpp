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

#include <atomic>
#include <mutex>
#include <queue>

#include "coke/mysql/mysql_client.h"
#include "coke/mysql/mysql_utils.h"

#include "workflow/StringUtil.h"
#include "workflow/WFTaskFactory.h"
#include "workflow/mysql_byteorder.h"

namespace coke {

MySQLClient::MySQLClient(const MySQLClientParams &p, bool unique_conn,
                         std::size_t conn_id)
    : unique_conn(unique_conn), conn_id(conn_id), params(p)
{
    std::string username, password, dbname, host;
    username = StringUtil::url_encode_component(params.username);
    password = StringUtil::url_encode_component(params.password);
    dbname = StringUtil::url_encode_component(params.dbname);

    url.assign(params.use_ssl ? "mysqls://" : "mysql://");

    // disable retry when use transaction
    if (unique_conn)
        params.retry_max = 0;

    if (!username.empty() || !password.empty())
        url.append(username).append(":").append(password).append("@");

    if (params.host.find(':') != std::string::npos &&
        params.host.front() != '[' && params.host.back() != ']')
    {
        host.reserve(params.host.size() + 2);
        host.append("[").append(params.host).append("]");
    }
    else
        host = params.host;

    url.append(host);

    if (params.port != 0)
        url.append(":").append(std::to_string(params.port));

    url.append("/").append(dbname);

    std::size_t pos = url.size();
    if (!params.character_set.empty())
        url.append("&character_set=").append(params.character_set);
    if (!params.character_set_results.empty())
        url.append("&character_set_results=")
            .append(params.character_set_results);

    if (unique_conn)
        url.append("&transaction=coke_mysql_transaction_id_")
            .append(std::to_string(conn_id));

    if (url.size() > pos)
        url[pos] = '?';

    URIParser::parse(url, uri);
}

MySQLClient::AwaiterType MySQLClient::request(const std::string &query)
{
    WFMySQLTask *task;

    task = WFTaskFactory::create_mysql_task(uri, params.retry_max, nullptr);
    task->set_send_timeout(params.send_timeout);
    task->set_receive_timeout(params.receive_timeout);
    task->set_keep_alive(params.keep_alive_timeout);
    task->get_req()->set_query(query);

    return AwaiterType(task);
}

struct __MySQLConnId {
    static __MySQLConnId *get_instance()
    {
        static __MySQLConnId instance;
        return &instance;
    }

    std::size_t acquire()
    {
        std::lock_guard<std::mutex> lg(mtx);

        if (que.empty())
            return uid++;

        std::size_t conn_id = que.top();
        que.pop();
        return conn_id;
    }

    void release(std::size_t cid)
    {
        std::lock_guard<std::mutex> lg(mtx);

        if (cid + 1 == uid)
            --uid;
        else
            que.push(cid);
    }

private:
    std::mutex mtx;
    std::size_t uid{0};
    std::priority_queue<std::size_t> que;
};

std::size_t MySQLConnection::acquire_conn_id()
{
    __MySQLConnId *p = __MySQLConnId::get_instance();
    return p->acquire();
}

void MySQLConnection::release_conn_id(std::size_t conn_id)
{
    __MySQLConnId *p = __MySQLConnId::get_instance();
    p->release(conn_id);
}

MySQLConnection::AwaiterType MySQLConnection::disconnect()
{
    WFMySQLTask *task;

    task = WFTaskFactory::create_mysql_task(uri, params.retry_max, nullptr);
    task->set_send_timeout(params.send_timeout);
    task->set_receive_timeout(params.receive_timeout);
    task->set_keep_alive(0);
    task->get_req()->set_query("");

    return AwaiterType(task);
}

void MySQLFieldView::reset(const char *buf, mysql_field_t *field)
{
    name = std::string_view(buf + field->name_offset, field->name_length);
    org_name = std::string_view(buf + field->org_name_offset,
                                field->org_name_length);
    table = std::string_view(buf + field->table_offset, field->table_length);
    org_table = std::string_view(buf + field->org_table_offset,
                                 field->org_table_length);
    db = std::string_view(buf + field->db_offset, field->db_length);
    catalog = std::string_view(buf + field->catalog_offset,
                               field->catalog_length);

    if (field->def_offset == std::size_t(-1) && field->def_length == 0)
        def = std::string_view();
    else
        def = std::string_view(buf + field->def_offset, field->def_length);

    flags = field->flags;
    length = field->length;
    decimals = field->decimals;
    charsetnr = field->charsetnr;
    data_type = field->data_type;
}

bool MySQLResultSetView::get_fields(std::vector<MySQLFieldView> &fields) const
{
    if (!is_result_set())
        return false;

    int field_count = get_field_count();
    fields.clear();
    fields.reserve(field_count);

    for (int i = 0; i < field_count; i++) {
        fields.emplace_back(buf, result_set->fields[i]);
    }

    return true;
}

bool MySQLResultSetView::next_row(std::vector<MySQLCellView> &cells)
{
    unsigned long long len;
    const unsigned char *data, *p;
    int dtype;

    if (off_cur == off_end)
        return false;

    p = off_cur;
    cells.clear();
    cells.reserve(result_set->field_count);

    for (int i = 0; i < result_set->field_count; i++) {
        dtype = result_set->fields[i]->data_type;

        if (*p == MYSQL_PACKET_HEADER_NULL) {
            p++;
            cells.emplace_back(MYSQL_TYPE_NULL, std::string_view());
        }
        else if (decode_string(&data, &len, &p, off_end) > 0) {
            const char *ptr = reinterpret_cast<const char *>(data);
            cells.emplace_back(dtype, std::string_view(ptr, len));
        }
        else {
            // error
            cells.clear();
            return false;
        }
    }

    off_cur = p;
    return true;
}

void MySQLResultSetView::rewind() noexcept
{
    if (is_result_set()) {
        const auto *p = reinterpret_cast<const unsigned char *>(buf);
        off_begin = p + result_set->rows_begin_offset;
        off_end = p + result_set->rows_end_offset;
        off_cur = off_begin;
    }
    else {
        off_begin = off_end = off_cur = nullptr;
    }
}

} // namespace coke
