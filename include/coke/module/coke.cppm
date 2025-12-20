module;

// clang-format off

#include "coke/basic_awaiter.h"
#include "coke/condition.h"
#include "coke/dag.h"
#include "coke/deque.h"
#include "coke/fileio.h"
#include "coke/future.h"
#include "coke/global.h"
#include "coke/go.h"
#include "coke/latch.h"
#include "coke/lru_cache.h"
#include "coke/make_task.h"
#include "coke/mutex.h"
#include "coke/qps_pool.h"
#include "coke/queue_common.h"
#include "coke/queue.h"
#include "coke/rlru_cache.h"
#include "coke/semaphore.h"
#include "coke/series.h"
#include "coke/shared_mutex.h"
#include "coke/sleep.h"
#include "coke/stop_token.h"
#include "coke/sync_guard.h"
#include "coke/task.h"
#include "coke/wait_group.h"
#include "coke/wait.h"

#include "coke/utils/hashtable.h"
#include "coke/utils/list.h"
#include "coke/utils/rbtree.h"
#include "coke/utils/str_holder.h"
#include "coke/utils/str_packer.h"

#include "coke/net/basic_server.h"
#include "coke/net/client_conn_info.h"
#include "coke/net/network.h"

#include "coke/http/http_client.h"
#include "coke/http/http_server.h"
#include "coke/http/http_utils.h"

#include "coke/mysql/mysql_client.h"
#include "coke/mysql/mysql_utils.h"

#include "coke/nspolicy/address_info.h"
#include "coke/nspolicy/nspolicy.h"
#include "coke/nspolicy/weighted_least_conn_policy.h"
#include "coke/nspolicy/weighted_policy_base_impl.h"
#include "coke/nspolicy/weighted_policy_base.h"
#include "coke/nspolicy/weighted_random_policy.h"
#include "coke/nspolicy/weighted_round_robin_policy.h"

#include "coke/redis/basic_types.h"
#include "coke/redis/client_impl.h"
#include "coke/redis/client_task.h"
#include "coke/redis/client.h"
#include "coke/redis/cluster_client_impl.h"
#include "coke/redis/cluster_client.h"
#include "coke/redis/message.h"
#include "coke/redis/options.h"
#include "coke/redis/parser.h"
#include "coke/redis/server.h"
#include "coke/redis/value.h"

#include "coke/tlv/basic_types.h"
#include "coke/tlv/client.h"
#include "coke/tlv/server.h"
#include "coke/tlv/task.h"

export module coke;

// Basic

export namespace coke {

// basic_awaiter.h

using coke::AwaiterInfo;
using coke::BasicAwaiter;

// condition.h

using coke::Condition;

// dag.h

using coke::dag_index_t;
using coke::dag_node_func_t;
using coke::DagBuilder;
using coke::DagGraph;
using coke::DagGraphBase;
using coke::DagNodeGroup;
using coke::DagNodeRef;
using coke::DagNodeVector;

using coke::operator>;
using coke::operator>=;

// deque.h

using coke::Deque;

// fileio.h

using coke::FileAwaiter;
using coke::FileResult;

using coke::pread;
using coke::pwrite;

using coke::preadv;
using coke::pwritev;

using coke::fsync;
using coke::fdatasync;

// future.h

using coke::Future;
using coke::Promise;

using coke::create_future;
using coke::wait_futures;
using coke::wait_futures_for;

// global.h

using coke::COKE_MAJOR_VERSION;
using coke::COKE_MINOR_VERSION;
using coke::COKE_PATCH_VERSION;

using coke::COKE_VERSION_STR;

using coke::STATE_UNDEFINED;
using coke::STATE_SUCCESS;
using coke::STATE_TOREPLY;
using coke::STATE_NOREPLY;
using coke::STATE_SYS_ERROR;
using coke::STATE_SSL_ERROR;
using coke::STATE_DNS_ERROR;
using coke::STATE_TASK_ERROR;
using coke::STATE_ABORTED;

using coke::CTOR_NOT_TIMEOUT;
using coke::CTOR_WAIT_TIMEOUT;
using coke::CTOR_CONNECT_TIMEOUT;
using coke::CTOR_TRANSMIT_TIMEOUT;

using coke::TOP_SUCCESS;
using coke::TOP_TIMEOUT;
using coke::TOP_ABORTED;
using coke::TOP_CLOSED;

using coke::INVALID_UNIQUE_ID;

using coke::EndpointParams;
using coke::GlobalSettings;

using coke::library_init;
using coke::get_error_string;

using coke::get_unique_id;
using coke::prevent_recursive_stack;

// go.h

using coke::GO_DEFAULT_QUEUE;

using coke::GoAwaiter;

using coke::go;
using coke::switch_go_thread;

// latch.h

using coke::LATCH_SUCCESS;
using coke::LATCH_TIMEOUT;

using coke::Latch;
using coke::LatchAwaiter;
using coke::SyncLatch;

// lru_cache.h

using coke::LruCache;

// make_task.h

using coke::make_task;

// mutex.h

using coke::Mutex;
using coke::UniqueLock;

// qps_pool.h

using coke::QpsPool;

// queue_common.h

using coke::QueueCommon;

// queue.h

using coke::Queue;
using coke::PriorityQueue;
using coke::Stack;

// rlru_cache.h

using coke::RlruCache;

// semaphore.h

using coke::Semaphore;

// series.h

using coke::SeriesAwaiter;
using coke::ParallelAwaiter;
using coke::EmptyAwaiter;

using coke::SeriesCreater;

using coke::current_series;
using coke::empty;
using coke::wait_parallel;
using coke::set_series_creater;
using coke::get_series_creater;
using coke::detach_on_series;
using coke::detach_on_new_series;

// shared_mutex.h

using coke::SharedMutex;
using coke::SharedLock;

// sleep.h

using coke::SLEEP_SUCCESS;
using coke::SLEEP_CANCELED;
using coke::SLEEP_ABORTED;

using coke::inf_dur;

using coke::NanoSec;
using coke::InfiniteDuration;
using coke::SleepAwaiter;
using coke::WFSleepAwaiter;

using coke::sleep;
using coke::yield;

using coke::cancel_sleep_by_id;
using coke::cancel_sleep_by_addr;
using coke::cancel_sleep_by_name;

// stop_token.h

using coke::StopToken;

// sync_guard.h

using coke::SyncGuard;

// task.h

using coke::is_task_v;

using coke::Task;
using coke::TaskRetType;

using coke::detach;

// wait_group.h

using coke::WAIT_GROUP_SUCCESS;

using coke::WaitGroupAwaiter;
using coke::WaitGroup;

// wait.h

using coke::sync_wait;
using coke::async_wait;
using coke::sync_call;

} // namespace coke

// Details

export namespace coke {

// detail/awaiter_base.h

using coke::AwaiterBase;

// detail/basic_concept.h

using coke::Cokeable;
using coke::Queueable;
using coke::IsCokePromise;
using coke::IsCokeAwaitable;

using coke::is_coke_awaitable_v;

// detail/future_base.h

using coke::_future_state;

// detail/random.h

using coke::rand_u64;

// detail/wait_helper.h

using coke::AwaitableType;
using coke::AwaiterResult;

using coke::make_task_from_awaitable;

} // namespace coke

// Utils

export namespace coke {

// utils/hashtable.h

using coke::HashtableNode;
using coke::Hashable;
using coke::EqualComparable;
using coke::HashEqualComparable;
using coke::Hashtable;

// utils/list.h

using coke::ListNode;
using coke::List;

// utils/rbtree.h

using coke::RBTreeNode;
using coke::RBComparable;
using coke::RBTree;

// utils/str_holder.h

using coke::StrHolder;
using coke::IndirectlyToStr;
using coke::StrHolderVec;

using coke::operator""_sv;
using coke::operator""_svh;

using coke::make_shv;
using coke::make_shv_view;

// utils/str_packer.h

using coke::StrPacker;

} // namespace coke

// Net

export namespace coke {

// net/basic_server.h

using coke::NetworkReplyResult;
using coke::NetworkReplyAwaiter;
using coke::ServerContext;
using coke::ServerParams;
using coke::BasicServer;

// net/client_conn_info.h

using coke::GENERIC_CLIENT_CONN_ID;
using coke::INVALID_CLIENT_INFO_ID;

using coke::ClientConnInfo;

// net/network.h

using coke::NetworkResult;
using coke::NetworkAwaiter;
using coke::SimpleNetworkAwaiter;

} // namespace coke

// HTTP

export namespace coke {

// http/http_client.h

using coke::HttpRequest;
using coke::HttpResponse;
using coke::HttpAwaiter;
using coke::HttpResult;

using coke::HttpClientParams;
using coke::HttpClient;

// http/http_server.h

using coke::HttpServerContext;
using coke::HttpReplyResult;

using coke::HttpServerParams;
using coke::HttpServer;

// http/http_utils.h

using coke::HttpMessage;
using coke::HttpHeaderView;
using coke::HttpHeaderCursor;
using coke::HttpChunkCursor;

using coke::http_body_view;

} // namespace coke

// MySQL

export namespace coke {

// mysql/mysql_client.h

using coke::MySQLRequest;
using coke::MySQLResponse;
using coke::MySQLAwaiter;
using coke::MySQLResult;

using coke::MySQLClientParams;
using coke::MySQLClient;
using coke::MySQLConnection;

// mysql/mysql_utils.h

using coke::MySQLMessage;
using coke::MySQLCellView;
using coke::MySQLFieldView;
using coke::MySQLResultSetView;
using coke::MySQLResultSetCursor;

using coke::mysql_datatype_to_str;

} // namespace coke

// Nspolicy

export namespace coke {

// nspolicy/address_info.h

using coke::ADDRESS_WEIGHT_MAX;

using coke::_addr_state;

using coke::AddressParams;
using coke::AddressPack;
using coke::HostPortPack;
using coke::AddressInfo;
using coke::AddressInfoDeleter;
using coke::HostPortRef;
using coke::AddressInfoCmp;

// nspolicy/nspolicy.h

using coke::NSPolicyParams;
using coke::NSPolicy;

// nspolicy/weighted_least_conn_policy.h

using coke::WeightedLeastConnPolicy;
using coke::WeightedLeastConnAddressInfo;

// nspolicy/weighted_policy_base.h

using coke::NSPolicyTrait;
using coke::WeightedPolicyBase;

// nspolicy/weighted_random_policy.h

using coke::WeightedRandomPolicy;
using coke::WeightedRandomAddressInfo;

// nspolicy/weighted_round_robin_policy.h

using coke::WeightedRoundRobinPolicy;
using coke::WeightedRoundRobinAddressInfo;

} // namespace coke

// Redis

export namespace coke {

// redis/commands/*

using coke::BitfieldBuilder;
using coke::BitfieldRoBuilder;
using coke::RedisBitmapCommands;

using coke::RedisRestoreOpt;
using coke::RedisScanOpt;
using coke::RedisGenericCommands;

using coke::RedisHexpireOpt;
using coke::RedisHscanOpt;
using coke::RedisHashCommands;

using coke::RedisHyperloglogCommands;

using coke::RedisListSideOpt;
using coke::RedisListPosOpt;
using coke::RedisLposOpt;
using coke::RedisListCommands;

using coke::RedisPublishCommands;

using coke::RedisSscanOpt;
using coke::RedisSetCommands;

using coke::RedisGetexExpireOpt;
using coke::RedisLcsOpt;
using coke::RedisSetOpt;
using coke::RedisStringCommands;

using coke::RedisTransactionCommands;

// redis/basic_types.h

using coke::RedisKeyValue;
using coke::RedisFieldElement;
using coke::RedisKeys;
using coke::RedisFields;
using coke::RedisElements;
using coke::RedisKeyValues;
using coke::RedisFieldElements;

using coke::_redis_flag;
using coke::_redis_slot;
using coke::_redis_err;

using coke::RedisExecuteOption;
using coke::RedisClientInfo;

// redis/client_impl.h

using coke::RedisClientParams;
using coke::RedisClientImpl;

// redis/client_task.h

using coke::RedisTask;
using coke::redis_callback_t;
using coke::RedisClientTask;
using coke::RedisAwaiter;

// redis/client.h

using coke::RedisClient;
using coke::RedisConnectionClient;

// redis/cluster_client_impl.h

using coke::RedisClusterClientParams;
using coke::RedisSlotNode;
using coke::RedisSlotNodes;
using coke::RedisSlotsTable;
using coke::RedisSlotsTablePtr;
using coke::RedisClusterClientImpl;

// redis/cluster_client.h

using coke::RedisClusterClient;

// redis/message.h

using coke::RedisMessage;
using coke::RedisRequest;
using coke::RedisResponse;

// redis/options.h

using coke::RedisOptsChoice;
using coke::RedisOptNone;
using coke::RedisOptStrBase;

using coke::RedisOptEx;
using coke::RedisOptPx;
using coke::RedisOptExat;
using coke::RedisOptPxat;
using coke::RedisOptPersist;
using coke::RedisOptKeepttl;
using coke::RedisOptNx;
using coke::RedisOptXx;
using coke::RedisOptGt;
using coke::RedisOptLt;
using coke::RedisOptFnx;
using coke::RedisOptFxx;
using coke::RedisOptLeft;
using coke::RedisOptRight;
using coke::RedisOptBefore;
using coke::RedisOptAfter;
using coke::RedisSetExpireOpt;
using coke::RedisExpireOpt;

// redis/parser.h

using coke::RedisParser;

// redis/server.h

using coke::RedisServerParams;
using coke::RedisServer;
using coke::RedisServerContext;
using coke::RedisProcessorType;

// redis/value.h

using coke::_redis_type;

using coke::RedisValue;
using coke::RedisNull;
using coke::RedisPair;
using coke::RedisArray;
using coke::RedisMap;
using coke::RedisVariant;
using coke::RedisResult;

using coke::make_redis_null;
using coke::make_redis_simple_string;
using coke::make_redis_bulk_string;
using coke::make_redis_verbatim_string;
using coke::make_redis_simple_error;
using coke::make_redis_bulk_error;
using coke::make_redis_big_number;
using coke::make_redis_integer;
using coke::make_redis_double;
using coke::make_redis_boolean;
using coke::make_redis_array;
using coke::make_redis_set;
using coke::make_redis_push;
using coke::make_redis_map;

} // namespace coke

// Tlv

export namespace coke {

// tlv/basic_types.h

using coke::TlvRequest;
using coke::TlvResponse;
using coke::TlvTask;
using coke::tlv_callback_t;

using coke::_tlv_err;
using coke::TlvClientInfo;
using coke::TlvResult;

// tlv/client.h

using coke::TlvClientParams;
using coke::TlvClient;
using coke::TlvConnectionClient;

// tlv/server.h

using coke::TlvServerParams;
using coke::TlvServer;
using coke::TlvServerContext;
using coke::TlvProcessorType;

// tlv/task.h

using coke::TlvClientTask;

} // namespace coke
