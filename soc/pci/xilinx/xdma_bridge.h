#ifndef XDMA_BRIDGE_H__
#define XDMA_BRIDGE_H__

#include <cassert>
#include <cstdint>
#include "sysc/communication/sc_mutex.h"
#include "sysc/communication/sc_signal_ports.h"
#include "sysc/datatypes/bit/sc_bv.h"
#include "sysc/datatypes/int/sc_nbdefs.h"
#include "sysc/kernel/sc_module.h"
#include "sysc/kernel/sc_module_name.h"
#include "sysc/kernel/sc_time.h"
#include "sysc/utils/sc_report.h"
#include "systemc.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
using namespace sc_core;

enum byp_ctl {
  RUNNING,
  STOP,
};

class tlm2xdma_desc_bypass_bridge : public sc_core::sc_module {
 public:
  tlm_utils::simple_initiator_socket<tlm2xdma_desc_bypass_bridge> init_socket;
  SC_HAS_PROCESS(tlm2xdma_desc_bypass_bridge);
  sc_in<bool> clk;
  sc_in<bool> resetn;
  sc_in<bool> load;
  sc_in<sc_bv<64>> src_addr;
  sc_in<uint32_t> len;
  sc_in<sc_bv<64>> dst_addr;
  sc_in<uint32_t> ctl;
  sc_out<bool> ready;

  tlm2xdma_desc_bypass_bridge(sc_core::sc_module_name name, bool h2c)
      : sc_module(name),
        init_socket("init_socket"),
        clk("clk"),
        resetn("resetn"),
        load("load"),
        src_addr("src_addr"),
        len("len"),
        dst_addr("dst_addr"),
        ctl("ctl"),
        ready("ready"),
        h2c(h2c) {
    SC_THREAD(handle_signal);
  }
  void handle_signal() {
    tlm::tlm_generic_payload gp;
    sc_time delay(SC_ZERO_TIME);
    if (h2c) {
      gp.set_command(tlm::TLM_READ_COMMAND);
    } else {
      gp.set_command(tlm::TLM_WRITE_COMMAND);
    }
    ready->write(true);
    while (true) {
      wait(clk->posedge_event());

      if (load->read() && ready->read()) {
        // unsigned int cmd = ctl->read();
        // sc_assert(cmd <= 4); // reserved
        // switch (cmd) {
        //     case byp_ctl::RUNNING:
        //         fetch_one_more_decriptor = true;
        //         break;
        //     case byp_ctl::STOP:
        //         fetch_one_more_decriptor = false;
        //         break;
        // }
        uint64_t dst_addr_read = dst_addr.read().to_uint64();
        uint64_t src_addr_read = src_addr.read().to_uint64();
        unsigned int length = len.read();
        gp.set_data_ptr(reinterpret_cast<unsigned char*>(src_addr_read));
        gp.set_address(dst_addr_read);
        gp.set_data_length(length);
        gp.set_streaming_width(length);
        init_socket->b_transport(gp, delay);
        ready.write(false);
        wait(clk->posedge_event());
        ready.write(true);
      }
    }
  }

 private:
  bool h2c;
};

#endif