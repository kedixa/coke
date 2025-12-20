/**
 * Copyright 2025 Coke Project (https://github.com/kedixa/coke)
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

#include <gtest/gtest.h>

#include "coke/redis/commands/bitmap.h"
#include "coke/redis/commands/generic.h"
#include "coke/redis/commands/hash.h"
#include "coke/redis/commands/hyperloglog.h"
#include "coke/redis/commands/list.h"
#include "coke/redis/commands/publish.h"
#include "coke/redis/commands/set.h"
#include "coke/redis/commands/string.h"
#include "coke/redis/commands/transaction.h"

class TestClient : public coke::RedisBitmapCommands<TestClient>,
                   public coke::RedisGenericCommands<TestClient>,
                   public coke::RedisHashCommands<TestClient>,
                   public coke::RedisHyperloglogCommands<TestClient>,
                   public coke::RedisListCommands<TestClient>,
                   public coke::RedisPublishCommands<TestClient>,
                   public coke::RedisSetCommands<TestClient>,
                   public coke::RedisStringCommands<TestClient>,
                   public coke::RedisTransactionCommands<TestClient> {
public:
    TestClient() = default;

    void check(coke::StrHolderVec command)
    {
        EXPECT_EQ(cmd.size(), command.size()) << cmd[0].as_view();

        if (cmd.size() == command.size()) {
            for (std::size_t i = 0; i < cmd.size(); i++) {
                EXPECT_EQ(cmd[i].as_view(), command[i].as_view());
            }
        }
    }

    TestClient &_execute(coke::StrHolderVec command, coke::RedisExecuteOption)
    {
        cmd = std::move(command);
        return *this;
    }

private:
    coke::StrHolderVec cmd;
};

TEST(REDIS_COMMAND, bitmap)
{
    TestClient cli;

    cli.bitcount("key").check({"BITCOUNT", "key"});

    cli.bitcount("key", 0, 10, true)
        .check({"BITCOUNT", "key", "0", "10", "BIT"});

    cli.bitfield(coke::BitfieldBuilder("key").get("u8", "0"))
        .check({"BITFIELD", "key", "GET", "u8", "0"});

    cli.bitfield_ro(coke::BitfieldRoBuilder("key").get("u8", "0"))
        .check({"BITFIELD_RO", "key", "GET", "u8", "0"});

    cli.bitop("AND", "destkey", {"key1", "key2"})
        .check({"BITOP", "AND", "destkey", "key1", "key2"});

    cli.bitpos("key", 1, 0, 10).check({"BITPOS", "key", "1", "0", "10"});

    cli.bitpos("key", 1, 0, 10, true)
        .check({"BITPOS", "key", "1", "0", "10", "BIT"});

    cli.getbit("key", 0).check({"GETBIT", "key", "0"});

    cli.setbit("key", 0, 1).check({"SETBIT", "key", "0", "1"});
}

TEST(REDIS_COMMAND, generic)
{
    TestClient cli;

    cli.copy("src_key", "dst_key").check({"COPY", "src_key", "dst_key"});

    cli.copy("src_key", "dst_key", 1, true)
        .check({"COPY", "src_key", "dst_key", "DB", "1", "REPLACE"});

    cli.del({"key1", "key2"}).check({"DEL", "key1", "key2"});

    cli.dump("key").check({"DUMP", "key"});

    cli.exists({"key1", "key2"}).check({"EXISTS", "key1", "key2"});

    cli.expire("key", 10, coke::RedisOptNx())
        .check({"EXPIRE", "key", "10", "NX"});

    cli.expireat("key", 1700000000, coke::RedisOptXx())
        .check({"EXPIREAT", "key", "1700000000", "XX"});

    cli.expiretime("key").check({"EXPIRETIME", "key"});

    cli.move("key", 1).check({"MOVE", "key", "1"});

    cli.object_encoding("key").check({"OBJECT", "ENCODING", "key"});

    cli.object_freq("key").check({"OBJECT", "FREQ", "key"});

    cli.object_idletime("key").check({"OBJECT", "IDLETIME", "key"});

    cli.object_refcount("key").check({"OBJECT", "REFCOUNT", "key"});

    cli.persist("key").check({"PERSIST", "key"});

    cli.pexpire("key", 1000, coke::RedisOptGt())
        .check({"PEXPIRE", "key", "1000", "GT"});

    cli.pexpireat("key", 1700000000000, coke::RedisOptLt())
        .check({"PEXPIREAT", "key", "1700000000000", "LT"});

    cli.pexpiretime("key").check({"PEXPIRETIME", "key"});

    cli.pttl("key").check({"PTTL", "key"});

    cli.randomkey().check({"RANDOMKEY"});

    cli.rename("old_key", "new_key").check({"RENAME", "old_key", "new_key"});

    cli.renamenx("old_key", "new_key")
        .check({"RENAMENX", "old_key", "new_key"});

    cli.restore("key", 100, "serialized_value")
        .check({"RESTORE", "key", "100", "serialized_value"});

    cli.scan("0", "pattern", 10, "string")
        .check(
            {"SCAN", "0", "MATCH", "pattern", "COUNT", "10", "TYPE", "string"});

    cli.touch({"key1", "key2"}).check({"TOUCH", "key1", "key2"});

    cli.ttl("key").check({"TTL", "key"});

    cli.type("key").check({"TYPE", "key"});

    cli.unlink({"key1", "key2"}).check({"UNLINK", "key1", "key2"});

    cli.time().check({"TIME"});

    cli.echo("message").check({"ECHO", "message"});

    cli.ping().check({"PING"});

    cli.ping("message").check({"PING", "message"});
}

TEST(REDIS_COMMAND, hash)
{
    TestClient cli;

    cli.hdel("key", {"field1", "field2"})
        .check({"HDEL", "key", "field1", "field2"});

    cli.hexists("key", "field").check({"HEXISTS", "key", "field"});

    cli.hexpire("key", 60, {"field1", "field2"}, coke::RedisOptNx())
        .check(
            {"HEXPIRE", "key", "60", "NX", "FIELDS", "2", "field1", "field2"});

    cli.hexpireat("key", 1700000000, {"field1", "field2"}, coke::RedisOptXx())
        .check({"HEXPIREAT", "key", "1700000000", "XX", "FIELDS", "2", "field1",
                "field2"});

    cli.hexpiretime("key", {"field1", "field2"})
        .check({"HEXPIRETIME", "key", "FIELDS", "2", "field1", "field2"});

    cli.hget("key", "field").check({"HGET", "key", "field"});

    cli.hgetall("key").check({"HGETALL", "key"});

    cli.hgetdel("key", {"field"})
        .check({"HGETDEL", "key", "FIELDS", "1", "field"});

    cli.hgetex("key", {"field1", "field2"}, coke::RedisOptEx{.seconds = 100})
        .check(
            {"HGETEX", "key", "EX", "100", "FIELDS", "2", "field1", "field2"});

    cli.hincrby("key", "field", 1).check({"HINCRBY", "key", "field", "1"});

    cli.hincrbyfloat("key", "field", 3.14)
        .check({"HINCRBYFLOAT", "key", "field", std::to_string(3.14)});

    cli.hkeys("key").check({"HKEYS", "key"});

    cli.hlen("key").check({"HLEN", "key"});

    cli.hmget("key", {"field1", "field2"})
        .check({"HMGET", "key", "field1", "field2"});

    cli.hmset("key", {{"field1", "value1"}, {"field2", "value2"}})
        .check({"HMSET", "key", "field1", "value1", "field2", "value2"});

    cli.hpersist("key", {"field1", "field2"})
        .check({"HPERSIST", "key", "FIELDS", "2", "field1", "field2"});

    cli.hpexpire("key", 1000, {"field1", "field2"}, coke::RedisOptXx{})
        .check({"HPEXPIRE", "key", "1000", "XX", "FIELDS", "2", "field1",
                "field2"});

    cli.hpexipreat("key", 1700000000000LL, {"field1", "field2"},
                   coke::RedisOptGt{})
        .check({"HPEXPIREAT", "key", "1700000000000", "GT", "FIELDS", "2",
                "field1", "field2"});

    cli.hpexpiretime("key", {"field1", "field2"})
        .check({"HPEXPIRETIME", "key", "FIELDS", "2", "field1", "field2"});

    cli.hpttl("key", {"field1", "field2"})
        .check({"HPTTL", "key", "FIELDS", "2", "field1", "field2"});

    cli.hrandfield("key").check({"HRANDFIELD", "key"});

    cli.hrandfield("key", 10, true)
        .check({"HRANDFIELD", "key", "10", "WITHVALUES"});

    cli.hscan("key", "0").check({"HSCAN", "key", "0"});

    cli.hset("key", {{"field", "element"}})
        .check({"HSET", "key", "field", "element"});

    cli.hsetex("key", {{"field1", "element1"}, {"field2", "element2"}},
               coke::RedisHsetexOpt{.exists = coke::RedisOptFnx{},
                                    .expire = coke::RedisOptKeepttl{}})
        .check({"HSETEX", "key", "FNX", "KEEPTTL", "FIELDS", "2", "field1",
                "element1", "field2", "element2"});

    cli.hsetnx("key", "field", "element")
        .check({"HSETNX", "key", "field", "element"});

    cli.hstrlen("key", "field").check({"HSTRLEN", "key", "field"});

    cli.httl("key", {"field1", "field2"})
        .check({"HTTL", "key", "FIELDS", "2", "field1", "field2"});

    cli.hvals("key").check({"HVALS", "key"});
}

TEST(REDIS_COMMAND, hyperloglog)
{
    TestClient cli;

    cli.pfadd("key", {"value1", "value2"})
        .check({"PFADD", "key", "value1", "value2"});

    cli.pfcount("key1", "key2").check({"PFCOUNT", "key1", "key2"});

    cli.pfmerge("dest_key", {"key1", "key2"})
        .check({"PFMERGE", "dest_key", "key1", "key2"});
}

TEST(REDIS_COMMAND, list)
{
    TestClient cli;

    cli.blmove("source", "destination", coke::RedisOptLeft{},
               coke::RedisOptRight{}, 1)
        .check({"BLMOVE", "source", "destination", "LEFT", "RIGHT",
                std::to_string(1.0)});

    cli.blmpop(1.0, {"key1", "key2"}, coke::RedisOptLeft{})
        .check({"BLMPOP", std::to_string(1.0), "2", "key1", "key2", "LEFT"});

    cli.blpop({"key1", "key2"}, 1.0)
        .check({"BLPOP", "key1", "key2", std::to_string(1.0)});

    cli.brpop({"key1", "key2"}, 1.0)
        .check({"BRPOP", "key1", "key2", std::to_string(1.0)});

    cli.brpoplpush("source", "destination", 1.0)
        .check({"BRPOPLPUSH", "source", "destination", std::to_string(1.0)});

    cli.lindex("key", 0).check({"LINDEX", "key", "0"});

    cli.linsert("key", coke::RedisOptBefore{}, "pivot", "value")
        .check({"LINSERT", "key", "BEFORE", "pivot", "value"});

    cli.llen("key").check({"LLEN", "key"});

    cli.lmove("source", "destination", coke::RedisOptLeft{},
              coke::RedisOptRight{})
        .check({"LMOVE", "source", "destination", "LEFT", "RIGHT"});

    cli.lmpop({"key1", "key2"}, coke::RedisOptLeft{})
        .check({"LMPOP", "2", "key1", "key2", "LEFT"});

    cli.lpop("key").check({"LPOP", "key"});

    cli.lpop("key", 10).check({"LPOP", "key", "10"});

    cli.lpos("key", "value").check({"LPOS", "key", "value"});

    cli.lpush("key", {"value1", "value2"})
        .check({"LPUSH", "key", "value1", "value2"});

    cli.lpushx("key", {"value1", "value2"})
        .check({"LPUSHX", "key", "value1", "value2"});

    cli.lrange("key", 0, 10).check({"LRANGE", "key", "0", "10"});

    cli.lrem("key", 1, "value").check({"LREM", "key", "1", "value"});

    cli.lset("key", 0, "value").check({"LSET", "key", "0", "value"});

    cli.ltrim("key", 0, 10).check({"LTRIM", "key", "0", "10"});

    cli.rpop("key").check({"RPOP", "key"});

    cli.rpop("key", 10).check({"RPOP", "key", "10"});

    cli.rpoplpush("source", "destination")
        .check({"RPOPLPUSH", "source", "destination"});

    cli.rpush("key", {"value1", "value2"})
        .check({"RPUSH", "key", "value1", "value2"});

    cli.rpushx("key", {"value1", "value2"})
        .check({"RPUSHX", "key", "value1", "value2"});
}

TEST(REDIS_COMMAND, publish)
{
    TestClient cli;

    cli.publish("channel", "message").check({"PUBLISH", "channel", "message"});

    cli.pubsub_channels("pattern").check({"PUBSUB", "CHANNELS", "pattern"});

    cli.pubsub_numpat().check({"PUBSUB", "NUMPAT"});

    cli.pubsub_numsub({"channel1", "channel2"})
        .check({"PUBSUB", "NUMSUB", "channel1", "channel2"});

    cli.spublish("channel", "message")
        .check({"SPUBLISH", "channel", "message"});
}

TEST(REDIS_COMMAND, set)
{
    TestClient cli;

    cli.sadd("key", {"member1", "member2"})
        .check({"SADD", "key", "member1", "member2"});

    cli.scard("key").check({"SCARD", "key"});

    cli.sdiff({"key1", "key2"}).check({"SDIFF", "key1", "key2"});

    cli.sdiffstore("dest_key", {"key1", "key2"})
        .check({"SDIFFSTORE", "dest_key", "key1", "key2"});

    cli.sinter({"key1", "key2"}).check({"SINTER", "key1", "key2"});

    cli.sintercard({"key1", "key2"}, 10)
        .check({"SINTERCARD", "2", "key1", "key2", "LIMIT", "10"});

    cli.sinterstore("dest_key", {"key1", "key2"})
        .check({"SINTERSTORE", "dest_key", "key1", "key2"});

    cli.sismember("key", "member").check({"SISMEMBER", "key", "member"});

    cli.smembers("key").check({"SMEMBERS", "key"});

    cli.smismember("key", {"member1", "member2"})
        .check({"SMISMEMBER", "key", "member1", "member2"});

    cli.smove("source_key", "dest_key", "member")
        .check({"SMOVE", "source_key", "dest_key", "member"});

    cli.spop("key").check({"SPOP", "key"});

    cli.spop("key", 10).check({"SPOP", "key", "10"});

    cli.srandmember("key").check({"SRANDMEMBER", "key"});

    cli.srandmember("key", 10).check({"SRANDMEMBER", "key", "10"});

    cli.srem("key", {"member1", "member2"})
        .check({"SREM", "key", "member1", "member2"});

    cli.sscan("key", "0").check({"SSCAN", "key", "0"});

    cli.sunion({"key1", "key2"}).check({"SUNION", "key1", "key2"});

    cli.sunionstore("dest_key", {"key1", "key2"})
        .check({"SUNIONSTORE", "dest_key", "key1", "key2"});
}

TEST(REDIS_COMMAND, string)
{
    TestClient cli;

    cli.append("key", "value").check({"APPEND", "key", "value"});

    cli.decr("key").check({"DECR", "key"});

    cli.decrby("key", 10).check({"DECRBY", "key", "10"});

    cli.get("key").check({"GET", "key"});

    cli.getdel("key").check({"GETDEL", "key"});

    cli.getex("key", coke::RedisOptEx{.seconds = 100})
        .check({"GETEX", "key", "EX", "100"});

    cli.getrange("key", 0, 10).check({"GETRANGE", "key", "0", "10"});

    cli.getset("key", "value").check({"GETSET", "key", "value"});

    cli.incr("key").check({"INCR", "key"});

    cli.incrby("key", 10).check({"INCRBY", "key", "10"});

    cli.incrbyfloat("key", 3.14)
        .check({"INCRBYFLOAT", "key", std::to_string(3.14)});

    cli.lcs("key1", "key2",
            {.len = true,
             .idx = true,
             .with_match_len = true,
             .min_match_len = 5})
        .check({"LCS", "key1", "key2", "LEN", "IDX", "MINMATCHLEN", "5",
                "WITHMATCHLEN"});

    cli.mget("key1", "key2").check({"MGET", "key1", "key2"});

    cli.mget(coke::StrHolderVec{"key1", "key2"})
        .check({"MGET", "key1", "key2"});

    cli.mset({{"key1", "value1"}, {"key2", "value2"}})
        .check({"MSET", "key1", "value1", "key2", "value2"});

    cli.mset("key1", "value1", "key2", "value2")
        .check({"MSET", "key1", "value1", "key2", "value2"});

    cli.msetnx({{"key1", "value1"}, {"key2", "value2"}})
        .check({"MSETNX", "key1", "value1", "key2", "value2"});

    cli.msetnx("key1", "value1", "key2", "value2")
        .check({"MSETNX", "key1", "value1", "key2", "value2"});

    cli.psetex("key", 1000, "value").check({"PSETEX", "key", "1000", "value"});

    cli.set("key", "value").check({"SET", "key", "value"});

    cli.set("key", "value",
            coke::RedisSetOpt{.get = true,
                              .exists = coke::RedisOptNx{},
                              .expire = coke::RedisOptEx{.seconds = 100}})
        .check({"SET", "key", "value", "GET", "NX", "EX", "100"});

    cli.setex("key", 100, "value").check({"SETEX", "key", "100", "value"});

    cli.setnx("key", "value").check({"SETNX", "key", "value"});

    cli.setrange("key", 0, "value").check({"SETRANGE", "key", "0", "value"});

    cli.strlen("key").check({"STRLEN", "key"});

    cli.substr("key", 0, 10).check({"SUBSTR", "key", "0", "10"});
}

TEST(REDIS_COMMAND, transaction)
{
    TestClient cli;

    cli.discard().check({"DISCARD"});

    cli.exec().check({"EXEC"});

    cli.multi().check({"MULTI"});

    cli.unwatch().check({"UNWATCH"});

    cli.watch({"key1", "key2"}).check({"WATCH", "key1", "key2"});
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
