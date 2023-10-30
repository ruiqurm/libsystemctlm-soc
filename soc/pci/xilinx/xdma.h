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
#include "sysc/kernel/sc_event.h"
#include "sysc/kernel/sc_module.h"
#include "sysc/kernel/sc_module_name.h"
#include "sysc/kernel/sc_object.h"
#include "sysc/kernel/sc_time.h"
#include "sysc/kernel/sc_wait.h"
#include "sysc/kernel/sc_wait_cthread.h"
#include "sysc/utils/sc_report.h"
#include "tlm-bridges/axis2tlm-bridge.h"
#include "tlm-bridges/tlm2axis-bridge.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "soc/pci/core/pci-device-base.h"
#include "systemc.h"
#include "xdma_bridge.h"
#include <deque>

using namespace sc_core;

#define XDMA_CONFIG_BAR_ID 0
#define XDMA_USER_BAR_ID 1
#define XDMA_TARGET_OFFSET 12

#define AXI_DATA_WIDTH 64 // 8 Byte
#define MAX_BYPASS_DSEC_QUEUE_SIZE 5

enum xdma_target{
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


class xilinx_xdma : public sc_module {
  SC_HAS_PROCESS(xilinx_xdma);

private:
  sc_fifo<xdma_bypass_desc*> c2h_queue;
  sc_fifo<xdma_bypass_desc*> h2c_queue;
  xdma_bypass_desc* current_c2h_desc;
public:
  sc_clock axi_clk;
  // TODO: 替换为握手
  // tlm2xdma_desc_bypass_bridge dsc_bypass_c2h_b;
  // tlm_utils::simple_target_socket<xilinx_xdma> dsc_bypass_c2h_proxy;
  
  // tlm2xdma_desc_bypass_bridge dsc_bypass_h2c_b;
  // tlm_utils::simple_target_socket<xilinx_xdma> dsc_bypass_h2c_proxy;
  tlm_utils::simple_target_socket<xilinx_xdma> dsc_bypass_c2h;
  tlm_utils::simple_target_socket<xilinx_xdma> dsc_bypass_h2c;

  // tlm2axis_bridge<AXI_DATA_WIDTH> m_axib;
  // axis2tlm_bridge<AXI_DATA_WIDTH> s_axib;
  tlm_utils::simple_target_socket<xilinx_xdma> m_axis_fwd;
  tlm_utils::simple_initiator_socket<xilinx_xdma> m_axis;
  // tlm_utils::simple_initiator_socket<xilinx_xdma> s_axis_fwd;
  tlm_utils::simple_target_socket<xilinx_xdma> s_axis;


  // tlm_utils::simple_target_socket<xilinx_xdma> user_bar;
  tlm_utils::simple_target_socket<xilinx_xdma> config_bar;
  tlm_utils::simple_initiator_socket<xilinx_xdma> dma; 
  tlm_utils::simple_initiator_socket<xilinx_xdma> card_bus;

  // sc_vector<sc_in<bool> > usr_irq_reqv;

  xilinx_xdma(sc_core::sc_module_name name)
      : axi_clk("clock-signal", sc_time(10, SC_NS)),
        // dsc_bypass_c2h_b("dsc-bypass-c2h-b"),
        // dsc_bypass_c2h_proxy("dsc-bypass-c2h-proxy"),
        // dsc_bypass_h2c_b("dsc-bypass-h2c-b"),
        // dsc_bypass_h2c_proxy("dsc-bypass-h2c-proxy"), 

        // m_axib("m-axib"),
        // s_axib("s-axib"), 
        m_axis_fwd("m-axis-fwd"),
        m_axis("m-axis"),
        s_axis("s-axis"),
        // user_bar("user-bar"), 
        config_bar("config-bar"),
        dma("dma"), card_bus("card-bus") {
    config_bar.register_b_transport(this, &xilinx_xdma::config_bar_b_transport);
    // user_bar.register_b_transport(this, &xilinx_xdma::user_bar_b_transport);

    ///  dsc_bypass_c2h_b <---- user logic h2c desc
    // dsc_bypass_c2h_b.init_socket.bind(dsc_bypass_c2h_proxy);
    // dsc_bypass_c2h_proxy.register_b_transport(
    //     this, &xilinx_xdma::tlm_desc_c2h_bypass_b_transport);

    ///  dsc_bypass_h2c_b <---- user logic c2h desc
    // dsc_bypass_h2c_b.init_socket.bind(dsc_bypass_h2c_proxy);
    // dsc_bypass_h2c_proxy.register_b_transport(
    //     this, &xilinx_xdma::tlm_desc_h2c_bypass_b_transport);

    dsc_bypass_c2h.register_b_transport(
        this, &xilinx_xdma::tlm_desc_c2h_bypass_b_transport);
    dsc_bypass_h2c.register_b_transport(
        this, &xilinx_xdma::tlm_desc_h2c_bypass_b_transport); 

    card_bus.bind(m_axis_fwd);
    m_axis_fwd.register_b_transport(this,&xilinx_xdma::m_axis_fwd_b_transport);
    s_axis.register_b_transport(this,&xilinx_xdma::s_axis_b_transport);

    SC_THREAD(handle_h2c_desc_bypass);
  }

  int do_h2c_dma(uint64_t src_addr, uint64_t dst_addr, uint64_t size) {
    uint64_t i;
    sc_time delay(SC_ZERO_TIME);
    tlm::tlm_generic_payload trans[2];
		genattr_extension *genattr = new genattr_extension();

    uint64_t data;

    trans[0].set_command(tlm::TLM_READ_COMMAND);
    trans[0].set_data_ptr((unsigned char *)&data);
    trans[0].set_streaming_width(8);
    trans[0].set_data_length(8);

    trans[1].set_command(tlm::TLM_WRITE_COMMAND);
    trans[1].set_data_ptr((unsigned char *)&data);
    trans[1].set_streaming_width(8);
    trans[1].set_data_length(8);
    trans[1].set_extension(genattr);

    for (i = 0; i < size; i += 8) {
      trans[0].set_address(src_addr);
      trans[1].set_address(dst_addr);
      src_addr += 8;
      dst_addr += 8;

      this->dma->b_transport(trans[0], delay);

      if (trans[0].get_response_status() != tlm::TLM_OK_RESPONSE) {
        SC_REPORT_ERROR("xdma", "error while fetching the data");
        return -1;
      }
      if (i + 8 >= size){
        genattr->set_eop();
      }else{
        genattr->set_eop(false);
      }
      this->card_bus->b_transport(trans[1], delay);

      if (trans[1].get_response_status() != tlm::TLM_OK_RESPONSE) {
        SC_REPORT_ERROR("xdma", "error while pushing the data");
        return -1;
      }
    }

    return 0;
  }

  // void reset(sc_signal<bool> resetn) {
  //   m_axib.resetn(resetn);
  //   s_axib.resetn(resetn);
  // }

private:
  void m_axis_fwd_b_transport(tlm::tlm_generic_payload &trans,sc_time &delay){
    m_axis->b_transport(trans,delay);
  }
  void handle_h2c_desc_bypass(void){
      while(true){
        xdma_bypass_desc* desc = h2c_queue.read();
        // print desc src addr,dst addr,len
        printf("src addr : %lld, dst addr : %lld, len : %lld\n", desc->src_addr, desc->dst_addr, desc->len);
        do_h2c_dma(desc->src_addr, desc->dst_addr, desc->len);
      }
  }

  void s_axis_b_transport(tlm::tlm_generic_payload &trans,sc_time &delay){
    uint8_t data[4096];
    tlm::tlm_generic_payload dma_trans;
    void * src_addr = trans.get_data_ptr();
    size_t length = trans.get_data_length();

    if (!current_c2h_desc){
      // not available
      current_c2h_desc = c2h_queue.read();
    }

    uint64_t dst_addr = current_c2h_desc->dst_addr;
    
    current_c2h_desc->len -= length;
    if (current_c2h_desc->len <= 0){
      delete current_c2h_desc;
      current_c2h_desc = nullptr;
    }else{
      current_c2h_desc->dst_addr += length;
    }

    // copy from AXI stream
    memcpy(&data, src_addr, length);

    // copy to address
    dma_trans.set_command(tlm::TLM_WRITE_COMMAND);
    dma_trans.set_data_ptr((unsigned char *)&data);
    dma_trans.set_streaming_width(length);
    dma_trans.set_data_length(length);
    dma_trans.set_address(dst_addr);

    // transmit to destination
    dma->b_transport(dma_trans,delay);
    if (dma_trans.get_response_status() != tlm::TLM_OK_RESPONSE){
      SC_REPORT_ERROR("xdma", "dma_proxy_b_transport:error while pushing the data");
      trans.set_response_status(dma_trans.get_response_status());
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
    xdma_bypass_desc* desc;
    // size_t streaming_width = trans.get_streaming_width();

    switch (command) {
      case tlm::TLM_WRITE_COMMAND:
        // Ignore src_addr here since source data from AXI stream
        desc = new xdma_bypass_desc;
        desc->src_addr = src_addr;
        desc->len = size;
        desc->dst_addr = dst_addr;
        c2h_queue.write(desc);
        break;
      default:
        SC_REPORT_ERROR("xdma", "tlm_desc_c2h_bypass_b_transport unknow command.");
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
    xdma_bypass_desc* desc;

    switch (command) {
      case tlm::TLM_READ_COMMAND:
        // ignore dst_addr here. since it's just a AXI stream.
        desc = new xdma_bypass_desc;
        desc->src_addr = src_addr;
        desc->len = size;
        desc->dst_addr = dst_addr;
        h2c_queue.write(desc);
        break;
      default:
        SC_REPORT_ERROR("xdma", "tlm_desc_h2c_bypass_b_transport unknow command.");
        break;
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
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
					v = 0x1fc20004;
					break;
        case xdma_target::CONFIG:
          v = 0x1fc30004;
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
		SC_REPORT_WARNING("xdma",
				"unsupported read / write on the config bar");
		trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
		return;
  }

  void user_bar_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay) {
    // we handle the user bar here
  }
};

class xdma_user_logic : public sc_module{
  public:
    SC_HAS_PROCESS(xdma_user_logic);
    tlm_utils::simple_initiator_socket<xdma_user_logic> c2h_data;
    tlm_utils::simple_target_socket<xdma_user_logic> h2c_data;

    tlm_utils::simple_initiator_socket<xdma_user_logic> c2h_desc;
    tlm_utils::simple_initiator_socket<xdma_user_logic> h2c_desc;
    tlm_utils::simple_target_socket<xdma_user_logic> user_bar;

    uint64_t src_addr;
    uint64_t dst_addr;

    uint32_t h2c_counter = 0;
    uint32_t descriptor_counter = 0;

    xdma_user_logic(sc_core::sc_module_name name):
      c2h_data("c2h-data"),
      h2c_data("h2c-data"),
      c2h_desc("c2h-desc"),
      h2c_desc("h2c-desc"),
      src_addr(0),
      dst_addr(0),
      descriptor_counter(0)
    {
      h2c_data.register_b_transport(this,&xdma_user_logic::h2c_data_b_transport);
      user_bar.register_b_transport(this,&xdma_user_logic::user_bar_b_transport);
      memset(data,0,4096);
      // SC_THREAD(send_c2h);
    }
    void user_bar_b_transport(tlm::tlm_generic_payload &trans,sc_time &delay) {
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
       switch(cmd){
          case tlm::TLM_READ_COMMAND:
            if (addr == 0){
              memcpy(data,&src_addr, 4);
            }else if(addr == 4){
              memcpy(data,(char*)&src_addr+4, 4);
            }else if (addr == 8){
              memcpy(data,&dst_addr, 4);
            }else if (addr == 12){
              memcpy(data,(char*)&dst_addr+4, 4);
            }else{
              SC_REPORT_ERROR("xdma", "user_bar_b_transport unknow address.");
            }
            break;
          case tlm::TLM_WRITE_COMMAND:
            if (addr == 0){
              memcpy(&src_addr,data, 4);
            }else if(addr == 4){
              memcpy((char*)&src_addr+4,data, 4);
            }else if (addr == 8){
              memcpy(&dst_addr,data, 4);
            }else if (addr == 12){
              memcpy((char*)&dst_addr+4,data, 4);
              send_h2c();
            }else{
              SC_REPORT_ERROR("xdma", "user_bar_b_transport unknow address.");
            }
            break;
          default:
            SC_REPORT_ERROR("xdma", "user_bar_b_transport unknow command.");
            break;
       }
       trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    void h2c_data_b_transport(tlm::tlm_generic_payload &trans,sc_time &delay) {
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
        // printf("descriptor_counter=%d, h2c_counter=%d, length=%d, eop=%d\n", descriptor_counter, h2c_counter, length, eop);
        memcpy(data + descriptor_counter*1024 + h2c_counter , ptr , length);
        h2c_counter += length;
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        if (eop){
          printf("eop = 1\n");
          h2c_counter = 0;
          descriptor_counter += 1;
          if (descriptor_counter == 4){
            display_data();
            send_c2h();
          }
        }
    }

    void send_h2c(){
      tlm::tlm_generic_payload desc;
      sc_time delay = SC_ZERO_TIME;
      desc.set_command(tlm::TLM_READ_COMMAND);
      desc.set_address(0X0);
      desc.set_streaming_width(1024);
      desc.set_data_length(1024);

      for(int i=0;i<4;i++){
        desc.set_data_ptr((unsigned char *)src_addr + 1024*i);
        h2c_desc->b_transport(desc,delay);
        if (desc.get_response_status() != tlm::TLM_OK_RESPONSE){
          SC_REPORT_ERROR("xdma", "dma_proxy_b_transport:error while pushing the data");
        }
      }

    }
    void send_c2h(){
      tlm::tlm_generic_payload desc;
      sc_time delay = SC_ZERO_TIME;
      desc.set_command(tlm::TLM_WRITE_COMMAND);
      desc.set_address(dst_addr);
      desc.set_data_ptr((unsigned char *)0x0);
      desc.set_streaming_width(4096);
      desc.set_data_length(4096);
      printf("send_c2h\n");
      c2h_desc->b_transport(desc,delay);
      if (desc.get_response_status() != tlm::TLM_OK_RESPONSE){
        SC_REPORT_ERROR("xdma", "dma_proxy_b_transport:error while pushing the data");
      }

      // write to a temp buffer
      desc.set_command(tlm::TLM_WRITE_COMMAND);
      desc.set_address(0);
      desc.set_streaming_width(8);
      desc.set_data_length(8);
      // display_data();
      for(int i = 0; i < 4096; i+=8){
        desc.set_data_ptr((unsigned char *)data+i);
        c2h_data->b_transport(desc,delay);
      }
    }
    void display_data(){
      for (int i = 0; i < 4096; ++i) {
        printf("%02x", (unsigned char)data[i] & 0xff);
      }
    }
  private:
    char data[4096];
};

// private:
//   void receive_bypass_descriptor() {
//     while (true) {
//       wait(clk.posedge_event());
//       c2h_dsc_byp_load.write(true);
//       // bool x = c2h_dsc_byp_ready.read();
//       // bool y = c2h_dsc_byp_load.read();
//       // printf("ready : %d, load : %d\n", x, y);
//       if (c2h_dsc_byp_ready.read() && c2h_dsc_byp_load->read()) {
//         // trans a descriptor
//         printf("send descriptor");
//         c2h_dsc_byp_src_addr.write(0x8000000);
//         c2h_dsc_byp_len.write(0x80);
//         c2h_dsc_byp_dst_addr.write(0x8000000);
//         c2h_dsc_byp_ctl.write(0x1);
//         wait(clk.posedge_event());
//         c2h_dsc_byp_load.write(false);
//       }
//     }
//   }
// };

#endif
