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

#include "sysc/kernel/sc_event.h"
#include "sysc/kernel/sc_module.h"
#include "sysc/kernel/sc_object.h"
#include "sysc/kernel/sc_time.h"
#include "sysc/kernel/sc_wait_cthread.h"
#include "sysc/utils/sc_report.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include <cstdio>
#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "soc/pci/core/pci-device-base.h"
#include "systemc.h"
#include "xdma_bridge.h"

#define XDMA_USER_BAR_ID 2

class xilinx_xdma : public sc_module {
  SC_HAS_PROCESS(xilinx_xdma);

private:
public:
  // we connect h2c_bridge directly ouside xdma
  tlm2xdma_h2c_byp_bridge h2c_bridge;
  tlm_utils::simple_initiator_socket<xilinx_xdma> tlm_h2c_byp;
  tlm2xdma_c2h_byp_bridge c2h_bridge;
  tlm_utils::simple_target_socket<xilinx_xdma> tlm_c2h_byp;

  tlm_utils::simple_target_socket<xilinx_xdma> user_bar;
  tlm_utils::simple_target_socket<xilinx_xdma> config_bar;
  tlm_utils::simple_initiator_socket<xilinx_xdma> dma;

  // sc_vector<sc_in<bool> > usr_irq_reqv;

  xilinx_xdma(sc_core::sc_module_name name)
      : h2c_bridge("h2c-bridge"), 
	  	tlm_h2c_byp("h2c-byp"),
	  	c2h_bridge("c2h-bridge"),
        tlm_c2h_byp("tlm-c2h-byp"), 
		user_bar("user-bar"),
        config_bar("config-bar") {
    config_bar.register_b_transport(this, &xilinx_xdma::config_bar_b_transport);
    user_bar.register_b_transport(this, &xilinx_xdma::user_bar_b_transport);
    tlm_c2h_byp.register_b_transport(this,
                                     &xilinx_xdma::tlm_c2h_byp_b_transport);
    c2h_bridge.init_socket.bind(tlm_c2h_byp);
	tlm_h2c_byp.bind(h2c_bridge.tgt_socket);
  }

  // void run(){
  // 	sc_time delay(SC_ZERO_TIME);
  // 	int i = 0;
  // 	while(i<2){
  // 		tlm::tlm_generic_payload trans[2];
  // 		wait(50	, sc_core::SC_SEC);
  // 		SC_REPORT_INFO("xdma", "write");
  // 		trans[1].set_command(tlm::TLM_WRITE_COMMAND);
  //         trans[1].set_data_ptr((unsigned char *)&data);
  //         trans[1].set_streaming_width(128);
  //         trans[1].set_data_length(128);
  // 		trans[1].set_address(0x8000000);
  // 		dma->b_transport(trans[1], delay);

  // 		if (trans[1].get_response_status() != tlm::TLM_OK_RESPONSE){
  // 			SC_REPORT_ERROR("xdma", "error 2");
  // 		}
  // 		SC_REPORT_INFO("xdma", "read");
  // 		trans[0].set_command(tlm::TLM_READ_COMMAND);
  //         trans[0].set_data_ptr((unsigned char *)&data);
  //         trans[0].set_streaming_width(128);
  //         trans[0].set_data_length(128);
  // 		trans[0].set_address(0x8000000); // 1G
  // 		dma->b_transport(trans[0],delay);

  // 		if (trans[0].get_response_status() != tlm::TLM_OK_RESPONSE){
  // 			SC_REPORT_ERROR("xdma", "error 1");
  // 		}
  // 		for(int x =0;x<128;x++){
  // 			printf("%02x",data[x]);
  // 		}
  // 		printf("\n");
  // 		wait(100, sc_core::SC_SEC);
  // 		i++;
  // 	}
  // }

  void reset(void) {}

private:
  // // Extends the PCI device base class forwarding BAR0 traffic
  // // onto the m_axib port.
  // void bar_b_transport(int bar_nr, tlm::tlm_generic_payload& trans,
  // 		     sc_time& delay) {
  // 	tlm_m_axib->b_transport(trans, delay);
  // }

  // void tlm_s_axib_b_transport(tlm::tlm_generic_payload& trans,
  // 			     sc_time& delay) {
  // 	dma->b_transport(trans, delay);
  // }

  void tlm_c2h_byp_b_transport(tlm::tlm_generic_payload &trans,
                               sc_time &delay) {
    dma->b_transport(trans, delay);
  }

  void config_bar_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay) {
    // we handle the config bar here
  }

  void user_bar_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay) {
    // we handle the user bar here
  }

  // Map usr_irq_reqv onto the PCI Base class.
  //
  // For now, this is a direct mapping on to the PCI base IRQ vector.
  // In the future we may want to implement our own mapping
  // to MSI/MSI-X without relying on QEMU doing that for us.
  // void handle_irqv(void) {
  // 	int i;

  // 	while (true) {
  // 		// Wait for sensitivity on any usr_irqv[]
  // 		wait();

  // 		for (i = 0; i < NUM_USR_IRQ; i++) {
  // 			irq[i] = usr_irq_reqv[i];
  // 		}
  // 	}
  // }
};

class xdma_user_logic : public sc_module {
public:
  SC_HAS_PROCESS(xdma_user_logic);
  sc_in<bool> clk;
  sc_out<bool> c2h_dsc_byp_load;
  sc_out<sc_bv<64>> c2h_dsc_byp_src_addr;
  sc_out<sc_bv<28>> c2h_dsc_byp_len;
  sc_out<sc_bv<64>> c2h_dsc_byp_dst_addr;
  sc_out<sc_bv<16>> c2h_dsc_byp_ctl;
  sc_in<bool> c2h_dsc_byp_ready;

  xdma_user_logic(sc_core::sc_module_name name) {
    // We do nothing currently
    // Mock a test in user logic later
  }

private:
  void receive_bypass_descriptor() {
    while (true) {
      wait(clk.posedge_event());
      c2h_dsc_byp_load.write(true);
      // bool x = c2h_dsc_byp_ready.read();
      // bool y = c2h_dsc_byp_load.read();
      // printf("ready : %d, load : %d\n", x, y);
      if (c2h_dsc_byp_ready.read() && c2h_dsc_byp_load->read()) {
        // trans a descriptor
        printf("send descriptor");
        c2h_dsc_byp_src_addr.write(0x8000000);
        c2h_dsc_byp_len.write(0x80);
        c2h_dsc_byp_dst_addr.write(0x8000000);
        c2h_dsc_byp_ctl.write(0x1);
        wait(clk.posedge_event());
        c2h_dsc_byp_load.write(false);
      }
    }
  }
};

#endif
