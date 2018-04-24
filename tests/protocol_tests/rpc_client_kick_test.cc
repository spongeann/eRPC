#include "protocol_tests.h"

namespace erpc {

// Ensure that we have enough packets to fill one credit window
static_assert(kTestLargeMsgSize / CTransport::kMTU > kSessionCredits, "");

/// Common setup code for client kick tests
class RpcClientKickTest : public RpcTest {
 public:
  SessionEndpoint client, server;
  Session *clt_session;
  SSlot *sslot_0;
  MsgBuffer req, resp;

  RpcClientKickTest() {
    client = get_local_endpoint();
    server = get_remote_endpoint();
    clt_session = create_client_session_connected(client, server);
    sslot_0 = &clt_session->sslot_arr[0];

    req = rpc->alloc_msg_buffer(kTestLargeMsgSize);
    resp = rpc->alloc_msg_buffer(kTestLargeMsgSize);
  }
};

/// Kicking a sslot without credits is disallowed
TEST_F(RpcClientKickTest, client_kick_st_no_credits) {
  rpc->enqueue_request(0, kTestReqType, &req, &resp, cont_func, kTestTag);
  assert(clt_session->client_info.credits == 0);
  ASSERT_DEATH(rpc->client_kick_st(sslot_0), ".*");
}

/// Kicking a sslot that has transmitted all request packets but received no
/// response packet is disallowed
TEST_F(RpcClientKickTest, client_kick_st_all_request_no_response) {
  rpc->enqueue_request(0, kTestReqType, &req, &resp, cont_func, kTestTag);
  assert(clt_session->client_info.credits == 0);
  sslot_0->client_info.num_tx = rpc->data_size_to_num_pkts(req.data_size);
  sslot_0->client_info.num_rx = sslot_0->client_info.num_tx - kSessionCredits;
  ASSERT_DEATH(rpc->client_kick_st(sslot_0), ".*");
}

/// Kicking a sslot that has received the full response is disallowed
TEST_F(RpcClientKickTest, client_kick_st_full_response) {
  rpc->enqueue_request(0, kTestReqType, &req, &resp, cont_func, kTestTag);
  assert(clt_session->client_info.credits == 0);

  *resp.get_pkthdr_0() = *req.get_pkthdr_0();  // Match request's formatted hdr

  sslot_0->client_info.num_tx = rpc->wire_pkts(&req, &resp);
  sslot_0->client_info.num_rx = rpc->wire_pkts(&req, &resp);
  clt_session->client_info.credits = kSessionCredits;
  sslot_0->client_info.resp_msgbuf = &resp;

  ASSERT_DEATH(rpc->client_kick_st(sslot_0), ".*");
}

/// Kick a sslot that hasn't transmitted all request packets
TEST_F(RpcClientKickTest, client_kick_st_req_pkts) {
  rpc->enqueue_request(0, kTestReqType, &req, &resp, cont_func, kTestTag);
  assert(clt_session->client_info.credits == 0);
  pkthdr_tx_queue->clear();

  // Pretend that an RFR has been received
  clt_session->client_info.credits = 1;
  sslot_0->client_info.num_rx = 1;
  rpc->client_kick_st(sslot_0);
  ASSERT_EQ(pkthdr_tx_queue->size(), 1);
  ASSERT_TRUE(
      pkthdr_tx_queue->pop().matches(PktType::kPktTypeReq, kSessionCredits));

  // Kicking twice in a row without any RX in between is disallowed
  ASSERT_DEATH(rpc->client_kick_st(sslot_0), ".*");
}

}  // End erpc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
