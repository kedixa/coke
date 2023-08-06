#include "coke/mysql_client.h"

#include "workflow/WFTaskFactory.h"

namespace coke {

MySQLClient::MySQLClient(const MySQLClientParams &params)
    : params(params)
{
    url.assign(params.use_ssl ? "mysqls://" : "mysql://");

    // TODO url encode
    if (!params.username.empty() || !params.password.empty())
        url.append(params.username).append(":")
           .append(params.password).append("@");

    url.append(params.host).append(":")
       .append(std::to_string(params.port)).append("/")
       .append(params.dbname);

    std::size_t pos = url.size();
    if (!params.character_set.empty())
        url.append("&character_set=").append(params.character_set);
    if (!params.character_set_results.empty())
        url.append("&character_set_results=").append(params.character_set_results);

    if (url.size() > pos)
        url[pos] = '?';
}

MySQLClient::AwaiterType
MySQLClient::request(const std::string &query) {
    WFMySQLTask *task;

    task = WFTaskFactory::create_mysql_task(url, params.retry_max, nullptr);
    task->set_send_timeout(params.send_timeout);
    task->set_receive_timeout(params.receive_timeout);
    task->set_keep_alive(params.keep_alive_timeout);
    task->get_req()->set_query(query);

    return AwaiterType(task);
}

} // namespace coke
