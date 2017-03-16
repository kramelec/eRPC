#ifndef ERPC_MSG_BUFFER_H
#define ERPC_MSG_BUFFER_H

#include "common.h"
#include "pkthdr.h"
#include "util/buffer.h"

namespace ERpc {

// Forward declarations
class IBTransport;

template <typename T>
class Rpc;

/// A message buffer with headers at the beginning and end
class MsgBuffer {
 public:
  friend class IBTransport;
  friend class Rpc<IBTransport>;

  MsgBuffer() {}
  ~MsgBuffer() {}

  /// Return an invalid MsgBuffer, i.e., \p buf is NULL.
  static inline MsgBuffer get_invalid_msgbuf() {
    MsgBuffer msg_buffer;
    msg_buffer.buf = nullptr;
    return msg_buffer;
  }

  /// Return a pointer to the pre-appended packet header of this MsgBuffer
  inline pkthdr_t *get_pkthdr_0() const {
    return (pkthdr_t *)(buf - sizeof(pkthdr_t));
  }

  /// Return a pointer to the nth (n >= 1) packet header of this MsgBuffer.
  /// This must use \p max_data_size, not \p data_size.
  inline pkthdr_t *get_pkthdr_n(size_t n) const {
    assert(n >= 1);
    return (pkthdr_t *)(buf + round_up<sizeof(size_t)>(max_data_size) +
                        (n - 1) * sizeof(pkthdr_t));
  }

  /// Check if a MsgBuffer is valid
  bool is_valid() const {
    if (buf == nullptr) {
      return false;
    }

    if (get_pkthdr_0()->magic != kPktHdrMagic) {
      return false;
    }

    return true;
  }

  /// Used by applications to get the current data size of a MsgBuffer
  inline size_t get_data_size() const { return data_size; }

  /// Return a string representation of this MsgBuffer
  std::string to_string() const {
    if (buf == nullptr) {
      return "[Invalid]";
    }

    std::ostringstream ret;
    ret << "[buf " << (void *)buf << ", "
        << "buffer " << buffer.to_string() << ", "
        << "data " << data_size << "(" << max_data_size << "), "
        << "pkts " << num_pkts << "(" << max_num_pkts << "), "
        << "pkts queued/rcvd " << pkts_queued << "]";
    return ret.str();
  }

 private:
  /// Construct a MsgBuffer with a valid Buffer allocated by eRPC.
  /// The zeroth packet header is stored at \p buffer.buf. \p buffer must have
  /// space for at least \p max_data_bytes, and \p max_num_pkts packet headers.
  MsgBuffer(Buffer buffer, size_t max_data_size, size_t max_num_pkts)
      : buf(buffer.buf + sizeof(pkthdr_t)),
        buffer(buffer),
        max_data_size(max_data_size),
        data_size(max_data_size),
        max_num_pkts(max_num_pkts),
        num_pkts(max_num_pkts),
        pkts_queued(0) {
    assert(buffer.buf != nullptr); /* buffer must be valid */
    /* data_size can be 0 */
    assert(max_num_pkts >= 1);
    assert(buffer.class_size >=
           max_data_size + max_num_pkts * sizeof(pkthdr_t));
  }

  /// Construct a single-packet MsgBuffer using a received packet.
  /// \p pkt must have space for \p max_data_bytes and one packet header.
  MsgBuffer(uint8_t *pkt, size_t max_data_size)
      : buf(pkt + sizeof(pkthdr_t)),
        buffer(Buffer::get_invalid_buffer()),
        max_data_size(max_data_size),
        data_size(max_data_size),
        max_num_pkts(1),
        num_pkts(1),
        pkts_queued(0) {
    assert(buf != nullptr);
    /* data_size can be zero */
  }

  /// Resize this MsgBuffer to any size smaller than its maximum allocation
  inline void resize(size_t new_data_size, size_t new_num_pkts) {
    assert(new_data_size <= max_data_size);
    assert(new_num_pkts <= max_num_pkts);
    data_size = new_data_size;
    num_pkts = new_num_pkts;
  }

 public:
  /// Pointer to the first *data* byte. (\p buffer.buf does not point to the
  /// first data byte.) The MsgBuffer is invalid if this is NULL.
  uint8_t *buf;

 private:
  Buffer buffer;  ///< The (optional) backing hugepage Buffer

  // Size info
  size_t max_data_size;  ///< Max data bytes in the MsgBuffer
  size_t data_size;      ///< Current data bytes in the MsgBuffer
  size_t max_num_pkts;   ///< Max number of packets in this MsgBuffer
  size_t num_pkts;       ///< Current number of packets in this MsgBuffer

  // Progress tracking info
  union {
    size_t pkts_queued;  ///< Packets queued for tx_burst
    size_t pkts_rcvd;    ///< Packets received from rx_burst
  };
};
}

#endif  // ERPC_MSG_BUFFER_H