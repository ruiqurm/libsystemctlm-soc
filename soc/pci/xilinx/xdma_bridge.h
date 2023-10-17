#ifndef XDMA_BRIDGE_H__
#define XDMA_BRIDGE_H__

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
#include <cassert>
#include <cstdint>
using namespace sc_core;

enum byp_ctl {
  RUNNING,
  STOP,
};

class tlm2xdma_c2h_byp_bridge : public sc_core::sc_module {
public:
  tlm_utils::simple_initiator_socket<tlm2xdma_c2h_byp_bridge> init_socket;
  SC_HAS_PROCESS(tlm2xdma_c2h_byp_bridge);
  sc_in<bool> clk;
  // In XDMA document diagram, resetn is not used. Maybe add it later?
  // sc_in<bool> resetn;
  sc_in<bool> dsc_byp_load;
  sc_in<sc_bv<64>> dsc_byp_src_addr;
  sc_in<sc_bv<28>> dsc_byp_len;
  sc_in<sc_bv<64>> dsc_byp_dst_addr;
  sc_in<sc_bv<16>> dsc_byp_ctl;
  sc_out<bool> dsc_byp_ready;

  tlm2xdma_c2h_byp_bridge(sc_core::sc_module_name name)
      : sc_module(name), init_socket("init_socket"),
        dsc_byp_load("dsc_byp_load"), dsc_byp_src_addr("dsc_byp_src_addr"),
        dsc_byp_len("dsc_byp_len"), dsc_byp_dst_addr("dsc_byp_dst_addr"),
        dsc_byp_ctl("dsc_byp_ctl"), dsc_byp_ready("dsc_byp_ready") {
    SC_THREAD(handle_signal);
  }
  void handle_signal() {
    tlm::tlm_generic_payload gp;
    sc_time delay(SC_ZERO_TIME);

    gp.set_command(tlm::TLM_WRITE_COMMAND);
    dsc_byp_ready->write(true);
    while (true) {
      wait(clk->posedge_event());

      if (dsc_byp_load->read() && dsc_byp_ready->read()) {
        unsigned int cmd = dsc_byp_ctl->read().to_uint();
        sc_assert(cmd <= 4); // reserved
        // TODO: We don't consider one more descriptor currently
        // switch (cmd) {
        //     case byp_ctl::RUNNING:
        //         fetch_one_more_decriptor = true;
        //         break;
        //     case byp_ctl::STOP:
        //         fetch_one_more_decriptor = false;
        //         break;
        // }
        uint64_t dst_addr = dsc_byp_dst_addr.read().to_uint64();
        uint64_t src_ddr = dsc_byp_src_addr.read().to_uint64();
        unsigned int length = dsc_byp_len.read().to_uint();
        gp.set_data_ptr(reinterpret_cast<unsigned char *>(src_ddr));
        gp.set_address(dst_addr);
        gp.set_data_length(length);
        gp.set_streaming_width(length);
        cout << gp.get_data_length() << endl;
        cout << gp.get_address() << endl;
        init_socket->b_transport(gp, delay);
        dsc_byp_ready.write(false);
        wait(clk->posedge_event());
        dsc_byp_ready.write(true);
      }
    }
  }

private:
};

class tlm2xdma_h2c_byp_bridge : public sc_core::sc_module {
public:
  tlm_utils::simple_target_socket<tlm2xdma_h2c_byp_bridge> tgt_socket;
  SC_HAS_PROCESS(tlm2xdma_h2c_byp_bridge);
  sc_in<bool> clk;
  sc_out<bool> dsc_byp_load;
  sc_out<sc_bv<64>> dsc_byp_src_addr;
  sc_out<sc_bv<28>> dsc_byp_len;
  sc_out<sc_bv<64>> dsc_byp_dst_addr;
  sc_out<sc_bv<16>> dsc_byp_ctl;
  sc_in<bool> dsc_byp_ready;
  tlm2xdma_h2c_byp_bridge(sc_core::sc_module_name name)
      : sc_module(name), tgt_socket("tgt_socket"), dsc_byp_load("dsc_byp_load"),
        dsc_byp_src_addr("dsc_byp_src_addr"), dsc_byp_len("dsc_byp_len"),
        dsc_byp_dst_addr("dsc_byp_dst_addr"), dsc_byp_ctl("dsc_byp_ctl"),
        dsc_byp_ready("dsc_byp_ready") {
    tgt_socket.register_b_transport(
        this, &tlm2xdma_h2c_byp_bridge::tgt_socket_b_transport);
  }
  void tgt_socket_b_transport(tlm::tlm_generic_payload &trans, sc_time &delay) {
    while (true) {
      wait(clk.posedge_event());
      dsc_byp_load.write(true);

      if (dsc_byp_ready.read() && dsc_byp_load->read()) {
        // after handshake, translate them to signals
        auto dst = trans.get_address();
        auto len = trans.get_data_length();
        auto src = trans.get_data_ptr();
        switch (trans.get_command()) {
        case tlm::TLM_WRITE_COMMAND:
          dsc_byp_src_addr.write((uint64_t)src);
          dsc_byp_len.write(len);
          dsc_byp_dst_addr.write(dst);
          dsc_byp_ctl.write(0x1);
          wait(clk.posedge_event());
          dsc_byp_load.write(false);
          break;

        default:
          SC_REPORT_ERROR("tlm2xdma_h2c_byp_bridge", "unsupported command");
        }
      }
    }
  }
};

#endif