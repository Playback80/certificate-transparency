#ifndef CERT_TRANS_UTIL_ETCD_H_
#define CERT_TRANS_UTIL_ETCD_H_

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <string>
#include <vector>

#include "base/macros.h"
#include "util/libevent_wrapper.h"
#include "util/status.h"
#include "util/task.h"

class JsonObject;

namespace cert_trans {


class EtcdClient {
 public:
  struct Node {
    static const Node& InvalidNode();

    Node() : Node(InvalidNode()) {
    }

    Node(int64_t created_index, int64_t modified_index, const std::string& key,
         const std::string& value);

    bool HasExpiry() const;

    std::string ToString() const;

    int64_t created_index_;
    int64_t modified_index_;
    std::string key_;
    std::string value_;
    std::chrono::system_clock::time_point expires_;
    bool deleted_;
  };

  struct WatchUpdate {
    WatchUpdate();
    WatchUpdate(const Node& node, const bool exists);

    const Node node_;
    const bool exists_;
  };

  typedef std::function<void(util::Status status, const EtcdClient::Node& node,
                             int64_t etcd_index)> GetCallback;
  typedef std::function<void(util::Status status,
                             const std::vector<EtcdClient::Node>& values,
                             int64_t etcd_index)> GetAllCallback;
  typedef std::function<void(util::Status status, int64_t index)>
      CreateCallback;
  typedef std::function<void(util::Status status, const std::string& key,
                             int64_t index)> CreateInQueueCallback;
  typedef std::function<void(util::Status status, int64_t new_index)>
      UpdateCallback;
  typedef std::function<void(util::Status status, int64_t new_index)>
      ForceSetCallback;
  typedef std::function<void(util::Status status, int64_t etcd_index)>
      DeleteCallback;
  typedef std::function<void(const std::vector<WatchUpdate>& updates)>
      WatchCallback;

  // TODO(pphaneuf): This should take a set of servers, not just one.
  EtcdClient(const std::shared_ptr<libevent::Base>& event_base,
             const std::string& host, uint16_t port);

  virtual ~EtcdClient();

  void Get(const std::string& key, const GetCallback& cb);

  void GetAll(const std::string& dir, const GetAllCallback& cb);

  void Create(const std::string& key, const std::string& value,
              const CreateCallback& cb);

  void CreateWithTTL(const std::string& key, const std::string& value,
                     const std::chrono::seconds& ttl,
                     const CreateCallback& cb);

  void CreateInQueue(const std::string& dir, const std::string& value,
                     const CreateInQueueCallback& cb);

  void Update(const std::string& key, const std::string& value,
              const int64_t previous_index, const UpdateCallback& cb);

  void UpdateWithTTL(const std::string& key, const std::string& value,
                     const std::chrono::seconds& ttl,
                     const int64_t previous_index, const UpdateCallback& cb);

  void ForceSet(const std::string& key, const std::string& value,
                const ForceSetCallback& cb);

  void ForceSetWithTTL(const std::string& key, const std::string& value,
                       const std::chrono::seconds& ttl,
                       const ForceSetCallback& cb);

  void Delete(const std::string& key, const int64_t current_index,
              const DeleteCallback& cb);

  virtual void Watch(const std::string& key, const WatchCallback& cb,
                     util::Task* task);

 protected:
  typedef std::function<void(util::Status status,
                             const std::shared_ptr<JsonObject>&,
                             int64_t etcd_index)> GenericCallback;

  EtcdClient() = default;  // Testing only

  virtual void Generic(const std::string& key,
                       const std::map<std::string, std::string>& params,
                       evhttp_cmd_type verb, const GenericCallback& cb);

 private:
  struct Request;
  struct WatchState;

  typedef std::map<std::pair<std::string, uint16_t>,
                   std::shared_ptr<libevent::HttpConnection> > ConnectionMap;

  // If MaybeUpdateLeader returns true, the handling of the response
  // should be aborted, as a new leader was found, and the request has
  // been retried on the new leader.
  bool MaybeUpdateLeader(const libevent::HttpRequest& req, Request* etcd_req);
  void RequestDone(const std::shared_ptr<libevent::HttpRequest>& req,
                   Request* etcd_req);

  std::shared_ptr<libevent::HttpConnection> GetConnection(
      const std::string& host, uint16_t port);

  std::shared_ptr<libevent::HttpConnection> GetLeader() const;
  std::shared_ptr<libevent::HttpConnection> UpdateLeader(
      const std::string& host, uint16_t port);

  const std::shared_ptr<libevent::Base> event_base_;

  mutable std::mutex lock_;
  ConnectionMap conns_;
  // Last known leader.
  std::shared_ptr<libevent::HttpConnection> leader_;

  DISALLOW_COPY_AND_ASSIGN(EtcdClient);
};


}  // namespace cert_trans

#endif  // CERT_TRANS_UTIL_ETCD_H_