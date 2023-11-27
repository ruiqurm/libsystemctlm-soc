/*
 * TLM-2.0 model of the Xilinx XDMA.
 *
 * Currently only supports PCIe-AXI brigde mode in tandem with QEMU.
 *
 * Copyright (c) 2020 Xilinx Inc.
 * Written by Edgar E. Iglesias.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef PCI_XILINX_XDMA_H__
#define PCI_XILINX_XDMA_H__

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <list>
#include "sysc/communication/sc_clock.h"
#include "sysc/communication/sc_fifo.h"
#include "sysc/datatypes/int/sc_nbdefs.h"
#include "sysc/kernel/sc_event.h"
#include "sysc/kernel/sc_module.h"
#include "sysc/kernel/sc_module_name.h"
#include "sysc/kernel/sc_object.h"
#include "sysc/kernel/sc_time.h"
#include "sysc/kernel/sc_wait.h"
#include "sysc/kernel/sc_wait_cthread.h"
#include "sysc/utils/sc_report.h"
#include "sysc/utils/sc_vector.h"
#include "tlm-bridges/axis2tlm-bridge.h"
#include "tlm-bridges/tlm2axilite-bridge.h"
#include "tlm-bridges/tlm2axis-bridge.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include "tlm_utils/multi_passthrough_target_socket.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "soc/pci/core/pci-device-base.h"
#include "systemc.h"
#include "xdma_bridge.h"

#define XDMA_CONFIG_BAR_ID 0
#define XDMA_USER_BAR_ID 1
#define XDMA_TARGET_OFFSET 12

#define AXI_DATA_WIDTH 256
#define AXI_DATA_WIDTH_BYTE (AXI_DATA_WIDTH / 8)
#define AXI_BRIDGE_STREAMING_WIDTH 64
#define MAX_BYPASS_DSEC_QUEUE_SIZE 1

enum xdma_target {
  H2C_CHANNELS = 0,
  C2H_CHANNELS,
  IRQ_BLOCK,
  CONFIG,
};

struct xdma_bypass_desc {
  uint64_t src_addr;
  uint64_t dst_addr;
  int64_t len;
};

template <typename H2C_BRIDGE, typename C2H_BRIDGE>
class xdma_descriptor_bypass : public sc_module {
 public:
  SC_HAS_PROCESS(xdma_descriptor_bypass);

  // Target socket for handling descriptor bypass
  tlm_utils::simple_target_socket<xdma_descriptor_bypass> dsc_bypass_c2h;
  tlm_utils::simple_target_socket<xdma_descriptor_bypass> dsc_bypass_h2c;

  // TLM bridge that converting descriptor bypass request to TLM2 packet.
  tlm2xdma_desc_bypass_bridge dsc_bypass_bridge_c2h;
  tlm2xdma_desc_bypass_bridge dsc_bypass_bridge_h2c;

  // DMA data transfer bridge. Convert TLM2 packet to AXI stream or vice versa.
  H2C_BRIDGE h2c_bridge;
  C2H_BRIDGE c2h_bridge;

  // Forward TLM2 packet to AXI bridge
  tlm_utils::simple_initiator_socket<xdma_descriptor_bypass> h2c_data;
  // Handling AXI stream from AXI bridge
  tlm_utils::simple_target_socket<xdma_descriptor_bypass> c2h_data;
  // Send TLM2 request to PCIe bus
  tlm_utils::simple_initiator_socket<xdma_descriptor_bypass> pcie_bus;

  explicit xdma_descriptor_bypass(sc_core::sc_module_name name)
      : sc_module(name),
        dsc_bypass_c2h("dsc-bypass-c2h"),
        dsc_bypass_h2c("dsc-bypass-h2c"),
        dsc_bypass_bridge_c2h("dsc-bypass-bridge-c2h", false),
        dsc_bypass_bridge_h2c("dsc-bypass-bridge-h2c", true),
        h2c_bridge("h2c-bridge"),
        c2h_bridge("c2h-bridge"),
        h2c_data("h2c_data"),
        c2h_data("c2h_data"),
        c2h_queue_event_("c2h-queue-event"),
        h2c_queue_event_("h2c_queue_event") {
    dsc_bypass_bridge_c2h.init_socket.bind(dsc_bypass_c2h);
    dsc_bypass_c2h.register_b_transport(
        this, &xdma_descriptor_bypass::tlm_desc_c2h_bypass_b_transport);
    dsc_bypass_bridge_h2c.init_socket.bind(dsc_bypass_h2c);
    dsc_bypass_h2c.register_b_transport(
        this, &xdma_descriptor_bypass::tlm_desc_h2c_bypass_b_transport);

    h2c_data.bind(h2c_bridge.tgt_socket);
    c2h_bridge.socket.bind(c2h_data);
    c2h_data.register_b_transport(
        this, &xdma_descriptor_bypass::c2h_data_b_transport);

    SC_THREAD(handle_h2c_desc_bypass);
  }
  void handle_h2c_desc_bypass() {
    char intermediate_buffer[AXI_DATA_WIDTH_BYTE];
    while (true) {
      uint64_t i;
      xdma_bypass_desc* desc;
      tlm::tlm_generic_payload trans[2];
      sc_time delay(SC_ZERO_TIME);
      auto* genattr = new genattr_extension();

      if (h2c_queue_.empty()) {
        sc_core::wait(h2c_queue_event_);
      }
      desc = h2c_queue_.front();
      size_t size = desc->len;
      uint64_t src_addr = desc->src_addr;
      uint64_t dst_addr = desc->dst_addr;

      trans[0].set_command(tlm::TLM_READ_COMMAND);
      trans[0].set_data_ptr(
          reinterpret_cast<unsigned char*>(&intermediate_buffer));
      trans[0].set_streaming_width(AXI_DATA_WIDTH_BYTE);
      trans[0].set_data_length(AXI_DATA_WIDTH_BYTE);

      trans[1].set_command(tlm::TLM_WRITE_COMMAND);
      trans[1].set_data_ptr(
          reinterpret_cast<unsigned char*>(&intermediate_buffer));
      trans[1].set_streaming_width(AXI_DATA_WIDTH_BYTE);
      trans[1].set_data_length(AXI_DATA_WIDTH_BYTE);
      trans[1].set_extension(genattr);

      for (i = 0; i < size; i += AXI_DATA_WIDTH_BYTE) {
        // check if is the last send
        // if so, raise eop
        if (i + AXI_DATA_WIDTH_BYTE >= size) {
          if (i + AXI_DATA_WIDTH_BYTE > size) {
            // unaligned visit
            int next_to_send_bytes = size - i;
            memset(intermediate_buffer, 0, sizeof(intermediate_buffer));
            trans[0].set_streaming_width(next_to_send_bytes);
            trans[0].set_data_length(next_to_send_bytes);
            trans[1].set_streaming_width(next_to_send_bytes);
            trans[1].set_data_length(next_to_send_bytes);
          }
          genattr->set_eop();
        } else {
          genattr->set_eop(false);
        }
        trans[0].set_address(src_addr);
        trans[1].set_address(dst_addr);

        this->pcie_bus->b_transport(trans[0], delay);

        if (trans[0].get_response_status() != tlm::TLM_OK_RESPONSE) {
          SC_REPORT_ERROR("xdma", "error while fetching the data");
        }
        this->h2c_data->b_transport(trans[1], delay);

        if (trans[1].get_response_status() != tlm::TLM_OK_RESPONSE) {
          SC_REPORT_ERROR("xdma", "error while pushing the data");
        }
        src_addr += AXI_DATA_WIDTH_BYTE;
        dst_addr += AXI_DATA_WIDTH_BYTE;
      }
      if (h2c_queue_.size() == MAX_BYPASS_DSEC_QUEUE_SIZE) {
        h2c_queue_full_event_.notify();
      }
      h2c_queue_.remove(desc);
      delete desc;
    }
  }

 private:
  sc_event c2h_queue_event_;
  sc_event h2c_queue_event_;
  sc_event c2h_queue_full_event_;
  sc_event h2c_queue_full_event_;
  std::list<xdma_bypass_desc*> c2h_queue_;
  std::list<xdma_bypass_desc*> h2c_queue_;

  void c2h_data_b_transport(tlm::tlm_generic_payload& trans, sc_time& delay) {
    uint8_t data[4096];
    tlm::tlm_generic_payload dma_trans;
    struct xdma_bypass_desc* current_c2h_desc;
    void* src_addr = trans.get_data_ptr();
    size_t length = trans.get_data_length();

    if (c2h_queue_.empty()) {
      sc_core::wait(c2h_queue_event_);
    }
    current_c2h_desc = c2h_queue_.front();

    uint64_t dst_addr = current_c2h_desc->dst_addr;

    current_c2h_desc->len -= length;

    // copy from AXI stream
    memcpy(&data, src_addr, length);

    // copy to address
    dma_trans.set_command(tlm::TLM_WRITE_COMMAND);
    dma_trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    dma_trans.set_streaming_width(length);
    dma_trans.set_data_length(length);
    dma_trans.set_address(dst_addr);

    // transmit to destination
    pcie_bus->b_transport(dma_trans, delay);
    if (dma_trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
      SC_REPORT_ERROR("xdma",
                      "dma_proxy_b_transport:error while pushing the data");
      trans.set_response_status(dma_trans.get_response_status());
    }

    if (current_c2h_desc->len <= 0) {
      if (c2h_queue_.size() == MAX_BYPASS_DSEC_QUEUE_SIZE) {
        c2h_queue_full_event_.notify();
      }
      c2h_queue_.remove(current_c2h_desc);
      delete current_c2h_desc;
    } else {
      current_c2h_desc->dst_addr += length;
    }

    // copy to host successfully
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
  }

  void tlm_desc_c2h_bypass_b_transport(tlm::tlm_generic_payload& trans,
                                       sc_time& /*delay*/) {
    int command = trans.get_command();
    auto src_addr = reinterpret_cast<uint64_t>(trans.get_data_ptr());
    uint64_t dst_addr = trans.get_address();
    unsigned int size = trans.get_data_length();
    xdma_bypass_desc* desc;

    switch (command) {
      case tlm::TLM_WRITE_COMMAND:
        // Ignore src_addr here since source data from AXI stream
        desc = new xdma_bypass_desc;
        desc->src_addr = src_addr;
        desc->len = size;
        desc->dst_addr = dst_addr;
        if (c2h_queue_.size() == MAX_BYPASS_DSEC_QUEUE_SIZE) {
          wait(c2h_queue_full_event_);
        }
        printf("c2h:desc->src_addr=%lx,desc->dst_addr=%lx,desc->len=%ld\n",
               desc->src_addr, desc->dst_addr, desc->len);
        c2h_queue_.push_back(desc);
        c2h_queue_event_.notify();
        break;
      default:
        SC_REPORT_ERROR("xdma",
                        "tlm_desc_c2h_bypass_b_transport unknow command.");
        break;
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
  }

  void tlm_desc_h2c_bypass_b_transport(tlm::tlm_generic_payload& trans,
                                       sc_time& /*delay*/) {
    int command = trans.get_command();
    auto src_addr = reinterpret_cast<uint64_t>(trans.get_data_ptr());
    uint64_t dst_addr = trans.get_address();
    unsigned int size = trans.get_data_length();
    xdma_bypass_desc* desc;

    switch (command) {
      case tlm::TLM_READ_COMMAND:
        desc = new xdma_bypass_desc;
        desc->src_addr = src_addr;
        desc->len = size;
        desc->dst_addr = dst_addr;
        if (h2c_queue_.size() == MAX_BYPASS_DSEC_QUEUE_SIZE) {
          wait(h2c_queue_full_event_);
        }
        printf("h2c: desc->src_addr=%lx,desc->dst_addr=%lx,desc->len=%ld\n",
               desc->src_addr, desc->dst_addr, desc->len);
        h2c_queue_.push_back(desc);
        h2c_queue_event_.notify();
        break;
      default:
        SC_REPORT_ERROR("xdma",
                        "tlm_desc_h2c_bypass_b_transport unknow command.");
        break;
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
  }
};

template <typename H2C_BRIDGE, typename C2H_BRIDGE>
class xilinx_xdma : public sc_module {
 private:
  sc_vector<tlm_utils::simple_target_socket_tagged<xilinx_xdma>>
      dma_arbitration_;
  struct {
    union {
      struct {
        uint32_t config_block_ident;
      };
      uint32_t u32[0x1000];
    } config_block;
    union {
      struct {
        uint32_t irq_block_ident;
      };
      uint32_t u32[0x1000];
    } irq_block;
  } regs_;

 public:
  SC_HAS_PROCESS(xilinx_xdma);
  tlm_utils::simple_target_socket<xilinx_xdma> config_bar;
  tlm_utils::simple_initiator_socket<xilinx_xdma> dmac;
  tlm2axilite_bridge<32, 32> user_bar;
  sc_vector<xdma_descriptor_bypass<H2C_BRIDGE, C2H_BRIDGE>>
      descriptor_bypass_channels;

  explicit xilinx_xdma(sc_core::sc_module_name name,
                       int descriptor_bypass_channel = 1)
      : sc_module(name),
        dma_arbitration_("descriptor-arbitration", descriptor_bypass_channel),
        config_bar("config-bar"),
        dmac("dmac"),
        user_bar("user-bar"),
        descriptor_bypass_channels("descriptor-bypass-channels",
                                   descriptor_bypass_channel) {
    config_bar.register_b_transport(this, &xilinx_xdma::config_bar_b_transport);
    for (int i = 0; i < descriptor_bypass_channel; i++) {
      dma_arbitration_[i].register_b_transport(
          this, &xilinx_xdma::dma_arbitration_b_transport, i);
    }
    for (int i = 0; i < descriptor_bypass_channel; i++) {
      descriptor_bypass_channels[i].pcie_bus.bind(dma_arbitration_[i]);
    }
  }

  void reset() {
    regs_.irq_block.irq_block_ident = 0x1fc20004;
    regs_.config_block.config_block_ident = 0x1fc30004;
  }

 private:
  void dma_arbitration_b_transport(int /*channel_id*/,
                                   tlm::tlm_generic_payload& trans,
                                   sc_time& delay) {
    dmac->b_transport(trans, delay);
  }
  void config_bar_b_transport(tlm::tlm_generic_payload& trans,
                              sc_time& /*delay*/) {
    tlm::tlm_command cmd = trans.get_command();
    unsigned char* data = trans.get_data_ptr();
    sc_dt::uint64 addr = trans.get_address();
    unsigned int len = trans.get_data_length();
    unsigned char* byte_en = trans.get_byte_enable_ptr();
    unsigned int s_width = trans.get_streaming_width();
    uint32_t v = 0;

    if (byte_en || len > 4 || s_width < len) {
      goto err;
    }
    if (cmd == tlm::TLM_READ_COMMAND) {
      switch (addr >> XDMA_TARGET_OFFSET) {
        case xdma_target::IRQ_BLOCK:
          v = regs_.irq_block.u32[(addr & 0xfff)];
          break;
        case xdma_target::CONFIG:
          v = regs_.config_block.u32[(addr & 0xfff)];
          break;
        default:
          goto err;
          break;
      }
      memcpy(data, &v, len);
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    return;
  err:
    SC_REPORT_WARNING("xdma", "unsupported read / write on the config bar");
    trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
  }
};

#endif