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
#include "tlm-bridges/tlm2axis-bridge.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include "tlm_utils/multi_passthrough_target_socket.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <list>
#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "soc/pci/core/pci-device-base.h"
#include "systemc.h"
#include "xdma_bridge.h"

using namespace sc_core;

#define XDMA_CONFIG_BAR_ID 0
#define XDMA_USER_BAR_ID 1
#define XDMA_TARGET_OFFSET 12

#define AXI_DATA_WIDTH 64 // 8 Byte
#define AXI_DATA_WIDTH_BYTE (AXI_DATA_WIDTH / 8)
#define AXI_BRIDGE_STREAMING_WIDTH 64
#define MAX_BYPASS_DSEC_QUEUE_SIZE 5

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

class xdma_descriptor_bypass : public sc_module {
public:
  SC_HAS_PROCESS(xdma_descriptor_bypass);
  // tlm2xdma_desc_bypass_bridge dsc_bypass_h2c_b;
  // tlm_utils::simple_target_socket<xdma_descriptor_bypass>
  // dsc_bypass_h2c_proxy;
  tlm_utils::simple_target_socket<xdma_descriptor_bypass> dsc_bypass_c2h;
  tlm_utils::simple_target_socket<xdma_descriptor_bypass> dsc_bypass_h2c;

  // tlm2axis_bridge<AXI_DATA_WIDTH> m_axib;
  // axis2tlm_bridge<AXI_DATA_WIDTH> s_axib;
  tlm_utils::simple_target_socket<xdma_descriptor_bypass> h2c_data_axis_fwd;
  tlm_utils::simple_initiator_socket<xdma_descriptor_bypass> h2c_data_axis;
  tlm_utils::simple_target_socket<xdma_descriptor_bypass> c2h_data_axis;

  tlm_utils::simple_initiator_socket<xdma_descriptor_bypass> card_bus;
  tlm_utils::simple_initiator_socket<xdma_descriptor_bypass> pcie_bus;

  xdma_descriptor_bypass(sc_core::sc_module_name name)
      : sc_module(name), dsc_bypass_c2h("dsc-bypass-c2h"),
        dsc_bypass_h2c("dsc-bypass-h2c"),
        h2c_data_axis_fwd("h2c-data-axis-fwd"), h2c_data_axis("h2c-data-axis"),
        c2h_data_axis("c2h-data-axis"), c2h_queue_event("c2h-queue-event"),
        h2c_queue_event("h2c_queue_event") {
    ///  dsc_bypass_c2h_b <---- user logic h2c desc
    // dsc_bypass_c2h_b.init_socket.bind(dsc_bypass_c2h_proxy);
    // dsc_bypass_c2h_proxy.register_b_transport(
    //     this, &xilinx_xdma::tlm_desc_c2h_bypass_b_transport);

    ///  dsc_bypass_h2c_b <---- user logic c2h desc
    // dsc_bypass_h2c_b.init_socket.bind(dsc_bypass_h2c_proxy);
    // dsc_bypass_h2c_proxy.register_b_transport(
    //     this, &xilinx_xdma::tlm_desc_h2c_bypass_b_transport);

    dsc_bypass_c2h.register_b_transport(
        this, &xdma_descriptor_bypass::tlm_desc_c2h_bypass_b_transport);
    dsc_bypass_h2c.register_b_transport(
        this, &xdma_descriptor_bypass::tlm_desc_h2c_bypass_b_transport);

    card_bus.bind(h2c_data_axis_fwd);
    h2c_data_axis_fwd.register_b_transport(
        this, &xdma_descriptor_bypass::h2c_data_axis_fwd_b_transport);
    c2h_data_axis.register_b_transport(
        this, &xdma_descriptor_bypass::c2h_data_axis_b_transport);

    SC_THREAD(handle_h2c_desc_bypass);
  }
  void handle_h2c_desc_bypass(void) {
    char intermediate_buffer[AXI_DATA_WIDTH_BYTE];
    while (true) {
      uint64_t i;
      xdma_bypass_desc *desc;
      tlm::tlm_generic_payload trans[2];
      sc_time delay(SC_ZERO_TIME);
      genattr_extension *genattr = new genattr_extension();

      if (h2c_queue.empty()) {
        sc_core::wait(h2c_queue_event);
      }
      desc = h2c_queue.front();
      size_t size = desc->len;
      uint64_t src_addr = desc->src_addr;
      uint64_t dst_addr = desc->dst_addr;

      trans[0].set_command(tlm::TLM_READ_COMMAND);
      trans[0].set_data_ptr((unsigned char *)&intermediate_buffer);
      trans[0].set_streaming_width(AXI_DATA_WIDTH_BYTE);
      trans[0].set_data_length(AXI_DATA_WIDTH_BYTE);

      trans[1].set_command(tlm::TLM_WRITE_COMMAND);
      trans[1].set_data_ptr((unsigned char *)&intermediate_buffer);
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
        this->card_bus->b_transport(trans[1], delay);

        if (trans[1].get_response_status() != tlm::TLM_OK_RESPONSE) {
          SC_REPORT_ERROR("xdma", "error while pushing the data");
        }
        src_addr += AXI_DATA_WIDTH_BYTE;
        dst_addr += AXI_DATA_WIDTH_BYTE;
      }
      do_h2c_dma(desc->src_addr, desc->dst_addr, desc->len);
      h2c_queue.remove(desc);
    }
  }

private:
  sc_event c2h_queue_event;
  sc_event h2c_queue_event;
  std::list<xdma_bypass_desc *> c2h_queue;
  std::list<xdma_bypass_desc *> h2c_queue;

  void h2c_data_axis_fwd_b_transport(tlm::tlm_generic_payload &trans,
                                     sc_time &delay) {
    h2c_data_axis->b_transport(trans, delay);
  }

  void c2h_data_axis_b_transport(tlm::tlm_generic_payload &trans,
                                 sc_time &delay) {
    uint8_t data[4096];
    tlm::tlm_generic_payload dma_trans;
    struct xdma_bypass_desc *current_c2h_desc;
    void *src_addr = trans.get_data_ptr();
    size_t length = trans.get_data_length();
    // size_t streaming_width = trans.get_streaming_width();

    if (c2h_queue.empty()) {
      // not available
      sc_core::wait(c2h_queue_event);
    }
    current_c2h_desc = c2h_queue.front();

    uint64_t dst_addr = current_c2h_desc->dst_addr;

    current_c2h_desc->len -= length;

    // copy from AXI stream
    memcpy(&data, src_addr, length);

    // copy to address
    dma_trans.set_command(tlm::TLM_WRITE_COMMAND);
    dma_trans.set_data_ptr((unsigned char *)&data);
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
      c2h_queue.remove(current_c2h_desc);
      delete current_c2h_desc;
    } else {
      current_c2h_desc->dst_addr += length;
    }

    // copy to host successfully
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
  }

  void tlm_desc_c2h_bypass_b_transport(tlm::tlm_generic_payload &trans,
                                       sc_time &delay) {
    int command = trans.get_command();
    uint64_t src_addr = (uint64_t)trans.get_data_ptr();
    uint64_t dst_addr = trans.get_address();
    unsigned int size = trans.get_data_length();
    xdma_bypass_desc *desc;
    // size_t streaming_width = trans.get_streaming_width();

    switch (command) {
    case tlm::TLM_WRITE_COMMAND:
      // Ignore src_addr here since source data from AXI stream
      desc = new xdma_bypass_desc;
      desc->src_addr = src_addr;
      desc->len = size;
      desc->dst_addr = dst_addr;
      c2h_queue.push_back(desc);
      c2h_queue_event.notify();
      break;
    default:
      SC_REPORT_ERROR("xdma",
                      "tlm_desc_c2h_bypass_b_transport unknow command.");
      break;
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
  }

  void tlm_desc_h2c_bypass_b_transport(tlm::tlm_generic_payload &trans,
                                       sc_time &delay) {
    int command = trans.get_command();
    uint64_t src_addr = (uint64_t)trans.get_data_ptr();
    uint64_t dst_addr = trans.get_address();
    unsigned int size = trans.get_data_length();
    xdma_bypass_desc *desc;

    switch (command) {
    case tlm::TLM_READ_COMMAND:
      desc = new xdma_bypass_desc;
      desc->src_addr = src_addr;
      desc->len = size;
      desc->dst_addr = dst_addr;
      h2c_queue.push_back(desc);
      h2c_queue_event.notify();
      break;
    default:
      SC_REPORT_ERROR("xdma",
                      "tlm_desc_h2c_bypass_b_transport unknow command.");
      break;
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
  }

  int do_h2c_dma(uint64_t src_addr, uint64_t dst_addr, uint64_t size) {

    return 0;
  }
};

class xilinx_xdma : public sc_module {
private:
  sc_vector<tlm_utils::simple_target_socket_tagged<xilinx_xdma>>
      dma_arbitration;
  // tlm_utils::simple_target_socket<xilinx_xdma> dma_arbitration;
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
  } regs;

public:
  SC_HAS_PROCESS(xilinx_xdma);
  // tlm_utils::simple_target_socket<xilinx_xdma> user_bar;
  tlm_utils::simple_target_socket<xilinx_xdma> config_bar;
  tlm_utils::simple_initiator_socket<xilinx_xdma> dmac;
  sc_vector<xdma_descriptor_bypass> descriptor_bypass_channels;
  // sc_vector<sc_in<bool> > usr_irq_reqv;

  xilinx_xdma(sc_core::sc_module_name name, int descriptor_bypass_channel = 1)
      : sc_module(name),
        dma_arbitration("descriptor-arbitration", descriptor_bypass_channel),
        config_bar("config-bar"), dmac("dmac"),
        descriptor_bypass_channels("descriptor-bypass-channels",
                                   descriptor_bypass_channel) {
    config_bar.register_b_transport(this, &xilinx_xdma::config_bar_b_transport);
    for (int i = 0; i < descriptor_bypass_channel; i++) {
      dma_arbitration[i].register_b_transport(
          this, &xilinx_xdma::dma_arbitration_b_transport, i);
    }
    for (int i = 0; i < descriptor_bypass_channel; i++) {
      descriptor_bypass_channels[i].pcie_bus.bind(dma_arbitration[i]);
    }
    // user_bar.register_b_transport(this, &xilinx_xdma::user_bar_b_transport);
  }

  void reset(void) {
    regs.irq_block.irq_block_ident = 0x1fc20004;
    regs.config_block.config_block_ident = 0x1fc30004;
  }

private:
  void dma_arbitration_b_transport(int channel_id,
                                   tlm::tlm_generic_payload &trans,
                                   sc_time &delay) {
    dmac->b_transport(trans, delay);
  }
  void config_bar_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay) {
    tlm::tlm_command cmd = trans.get_command();
    unsigned char *data = trans.get_data_ptr();
    sc_dt::uint64 addr = trans.get_address();
    unsigned int len = trans.get_data_length();
    unsigned char *byte_en = trans.get_byte_enable_ptr();
    unsigned int s_width = trans.get_streaming_width();
    uint32_t v = 0;

    if (byte_en || len > 4 || s_width < len) {
      goto err;
    }
    if (cmd == tlm::TLM_READ_COMMAND) {
      switch (addr >> XDMA_TARGET_OFFSET) {
      case xdma_target::IRQ_BLOCK:
        v = regs.irq_block.u32[(addr & 0xfff)];
        break;
      case xdma_target::CONFIG:
        v = regs.config_block.u32[(addr & 0xfff)];
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
    return;
  }

  void user_bar_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay) {
    // we handle the user bar here
  }
};

/*
 *
 * Example of User Logic
 *
 */
#define USER_LOGIC_CARD_BUFFER 4096
#define USER_LOGIC_COMMAND_SRC_ADDR_LOW 0x0
#define USER_LOGIC_COMMAND_SRC_ADDR_HIGH 0x4
#define USER_LOGIC_COMMAND_DST_ADDR_LOW 0x8
#define USER_LOGIC_COMMAND_DST_ADDR_HIGH 0xc

class user_logic_desc_bypass : public sc_module {
public:
  SC_HAS_PROCESS(user_logic_desc_bypass);
  tlm_utils::simple_initiator_socket<user_logic_desc_bypass> c2h_data;
  tlm_utils::simple_target_socket<user_logic_desc_bypass> h2c_data;

  tlm_utils::simple_initiator_socket<user_logic_desc_bypass> c2h_desc;
  tlm_utils::simple_initiator_socket<user_logic_desc_bypass> h2c_desc;

  char *data;
  uint64_t dst_addr;
  int h2c_counter;
  size_t DATA_LENGTH;
  explicit user_logic_desc_bypass(sc_core::sc_module_name name)
      : c2h_data("c2h-data"), h2c_data("h2c-data"), c2h_desc("c2h-desc"),
        h2c_desc("h2c-desc"), h2c_counter(0) {
    h2c_data.register_b_transport(
        this, &user_logic_desc_bypass::h2c_data_b_transport);
  }

  void h2c_data_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay) {
    genattr_extension *genattr;
    bool eop = true;
    auto ptr = trans.get_data_ptr();
    auto length = trans.get_data_length();
    trans.get_extension(genattr);
    if (genattr) {
      eop = genattr->get_eop();
    }

    // print debug info
    assert(length <= AXI_DATA_WIDTH / 8);
    // printf("descriptor_counter=%d, h2c_counter=%d, length=%d, eop=%d\n",
    // descriptor_counter, h2c_counter, length, eop);
    memcpy(data + h2c_counter, ptr, length);
    h2c_counter += length;
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    if (eop) {
      h2c_counter = 0;
      send_c2h();
    }
  }

  void send_h2c(uint64_t src_addr, char *data, uint64_t dst_addr,
                size_t data_length) {
    tlm::tlm_generic_payload desc;

    this->data = data;
    this->dst_addr = dst_addr;
    this->DATA_LENGTH = data_length;

    sc_time delay = SC_ZERO_TIME;
    desc.set_command(tlm::TLM_READ_COMMAND);
    desc.set_address((uint64_t)data);
    desc.set_streaming_width(AXI_BRIDGE_STREAMING_WIDTH);
    desc.set_data_length(DATA_LENGTH);
    desc.set_data_ptr((unsigned char *)src_addr);
    h2c_desc->b_transport(desc, delay);
    if (desc.get_response_status() != tlm::TLM_OK_RESPONSE) {
      SC_REPORT_ERROR("xdma",
                      "dma_proxy_b_transport:error while pushing the data");
    }
  }
  void send_c2h() {
    tlm::tlm_generic_payload desc, data_tlm;
    sc_time delay = SC_ZERO_TIME;
    desc.set_command(tlm::TLM_WRITE_COMMAND);
    desc.set_address(dst_addr);
    desc.set_data_ptr((unsigned char *)0x0);
    desc.set_streaming_width(AXI_BRIDGE_STREAMING_WIDTH);
    desc.set_data_length(DATA_LENGTH);
    printf("send_c2h\n");
    c2h_desc->b_transport(desc, delay);
    if (desc.get_response_status() != tlm::TLM_OK_RESPONSE) {
      SC_REPORT_ERROR("xdma",
                      "dma_proxy_b_transport:error while pushing the data");
    }

    // write to a temp buffer
    data_tlm.set_command(tlm::TLM_WRITE_COMMAND);
    data_tlm.set_address(0);
    data_tlm.set_streaming_width(AXI_DATA_WIDTH_BYTE);
    data_tlm.set_data_length(DATA_LENGTH);
    data_tlm.set_data_ptr((unsigned char *)data);
    c2h_data->b_transport(data_tlm, delay);
  }
};

class xdma_user_logic : public sc_module {
public:
  SC_HAS_PROCESS(xdma_user_logic);
  // tlm_utils::simple_initiator_socket<xdma_user_logic> c2h_data;
  // tlm_utils::simple_target_socket<xdma_user_logic> h2c_data;

  // tlm_utils::simple_initiator_socket<xdma_user_logic> c2h_desc;
  // tlm_utils::simple_initiator_socket<xdma_user_logic> h2c_desc;
  sc_vector<user_logic_desc_bypass> desc_bypass_channels;
  tlm_utils::simple_target_socket<xdma_user_logic> user_bar;

  uint64_t src_addr;
  uint64_t dst_addr;

  uint32_t h2c_counter = 0;
  int number_of_desc_channels;
  xdma_user_logic(sc_core::sc_module_name name, int number_of_desc_channels)
      : desc_bypass_channels("desc-bypass-channels", number_of_desc_channels),
        src_addr(0), dst_addr(0),
        number_of_desc_channels(number_of_desc_channels) {
    user_bar.register_b_transport(this, &xdma_user_logic::user_bar_b_transport);
    memset(card_memory, 0, 4096);
  }
  void user_bar_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay) {
    auto cmd = trans.get_command();
    auto data = trans.get_data_ptr();
    auto addr = trans.get_address();
    auto length = trans.get_data_length();
    if (length != 4) {
      SC_REPORT_ERROR("xdma", "user_bar_b_transport: length must be 4");
      trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
      return;
    }
    // print cmd and addr
    printf("cmd : %d, addr : %lld\n", cmd, addr);
    switch (cmd) {
    case tlm::TLM_READ_COMMAND:
      if (addr == USER_LOGIC_COMMAND_SRC_ADDR_LOW) {
        memcpy(data, &src_addr, 4);
      } else if (addr == USER_LOGIC_COMMAND_SRC_ADDR_HIGH) {
        memcpy(data, (char *)&src_addr + 4, 4);
      } else if (addr == USER_LOGIC_COMMAND_DST_ADDR_LOW) {
        memcpy(data, &dst_addr, 4);
      } else if (addr == USER_LOGIC_COMMAND_DST_ADDR_HIGH) {
        memcpy(data, (char *)&dst_addr + 4, 4);
      } else {
        SC_REPORT_ERROR("xdma", "user_bar_b_transport unknow address.");
      }
      break;
    case tlm::TLM_WRITE_COMMAND:
      if (addr == USER_LOGIC_COMMAND_SRC_ADDR_LOW) {
        memcpy(&src_addr, data, 4);
      } else if (addr == USER_LOGIC_COMMAND_SRC_ADDR_HIGH) {
        memcpy((char *)&src_addr + 4, data, 4);
      } else if (addr == USER_LOGIC_COMMAND_DST_ADDR_LOW) {
        memcpy(&dst_addr, data, 4);
      } else if (addr == USER_LOGIC_COMMAND_DST_ADDR_HIGH) {
        memcpy((char *)&dst_addr + 4, data, 4);
        for (int channel_id = 0; channel_id < number_of_desc_channels;
             channel_id++) {
          auto offset = channel_id * 1024;
          // printf("channel_id : %d, offset : %d,length=%d\n", channel_id,
          // offset,1024);
          desc_bypass_channels[channel_id].send_h2c(
              src_addr + offset, card_memory + offset, dst_addr + offset, 1020);
        }
      } else {
        SC_REPORT_ERROR("xdma", "user_bar_b_transport unknow address.");
      }
      break;
    default:
      SC_REPORT_ERROR("xdma", "user_bar_b_transport unknow command.");
      break;
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
  }

private:
  char card_memory[USER_LOGIC_CARD_BUFFER];
};

#endif