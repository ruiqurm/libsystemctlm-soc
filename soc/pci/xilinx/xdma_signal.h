#ifndef XDMA_SIGNAL_H__
#define XDMA_SIGNAL_H__

#include <cstdint>
#include "sysc/communication/sc_clock.h"
#include "sysc/communication/sc_signal.h"
#include "sysc/communication/sc_signal_ports.h"
#include "sysc/datatypes/bit/sc_bv.h"
#include "sysc/kernel/sc_module.h"
#include "systemc.h"
#include "tlm-bridges/amba.h"
#include "xdma.h"

/*
Common length of AXI signals
*/
#define AXIL_AWPROT_WIDTH 3
#define AXIL_ARPROT_WIDTH 3
#define AXIL_WSTRB_WIDTH 4
#define AXIL_BRESP_WIDTH 2
#define AXIL_RRESP_WIDTH 2

#define AXI_AWPROT_WIDTH 4
#define AXI_ARPROT_WIDTH 3
#define AXI_REGION_WIDTH 4
#define AXI_QOS_WIDTH 4
#define AXI_CACHE_WIDTH 4
#define AXI_BURST_WIDTH 2
#define AXI_SIZE_WIDTH 3  // AW_SIZE & AR_SIZE
#define AXI_LEN_WIDTH 8   // AW_LEN & AR_LEN
#define AXI_ID_WIDTH 8    // AW_ID & AR_ID & W_ID & B_ID & R_ID
#define AXI_LOCK_WIDTH 1
#define AXI_ADDR_WIDTH 32
#define AXI_USER_WIDTH 2
#define AXI_RESP_WIDTH 2  // B_RESP & R_RESP
#define AXI_BAR_WIDTH 2
#define AXI_DOMAIN_WIDTH 2
#define AXI_AWSNOOP_WIDTH 3
#define AXI_ARSNOOP_WIDTH 3

/*
User defined length of AXI signals
Note: Should be edited by script
*/
#define BRIDGE_ADDR_WIDTH 32
#define BRIDGE_DATA_WIDTH 32
#define DMA_DATA_WIDTH 256
#define DMA_ADDR_WIDTH 64
#define DMA_DATA_WIDTH_IN_BYTES 32

using namespace sc_core;

// Note thate OUT_TYPE means sc_out<OUT_TYPE>, which needs to be sent to pin `in`,vice versa.
template <typename OUT_TYPE, typename IN_TYPE, IN_TYPE (*converter)(OUT_TYPE)>
class type_adapter : public sc_core::sc_module {
 public:
  SC_HAS_PROCESS(type_adapter);
  sc_signal<OUT_TYPE> out_pin_signal;
  sc_signal<IN_TYPE> in_pin_signal;
  type_adapter(sc_core::sc_module_name name) : sc_module(name) {
    SC_METHOD(convert);
    sensitive << in;
    in(out_pin_signal);
    out(in_pin_signal);
  }

 private:
  sc_in<OUT_TYPE> in;
  sc_out<IN_TYPE> out;
  void convert() { out.write(converter(in.read())); }
};

template <unsigned int N>
inline sc_bv<N> uint32t_to_bvn(uint32_t in) {
  sc_bv<N> out;
  out = in;
  return out;
}
template <unsigned int N>
inline uint32_t bvn_to_uint32t(sc_bv<N> in) {
  return in.to_uint();
}

class xdma_bypass_signal : public sc_core::sc_module {
 public:
  sc_signal<bool> xdmaChannel_h2cDescByp_load;
  sc_signal<sc_bv<DMA_ADDR_WIDTH>> xdmaChannel_h2cDescByp_src_addr;
  sc_signal<uint32_t> xdmaChannel_h2cDescByp_len;
  sc_signal<sc_bv<DMA_ADDR_WIDTH>> xdmaChannel_h2cDescByp_dst_addr;
  sc_signal<uint32_t> xdmaChannel_h2cDescByp_ctl;
  sc_signal<bool> xdmaChannel_h2cDescByp_ready;
  sc_signal<bool> xdmaChannel_h2cDescByp_desc_done;

  sc_signal<bool> xdmaChannel_c2hDescByp_load;
  sc_signal<sc_bv<DMA_ADDR_WIDTH>> xdmaChannel_c2hDescByp_src_addr;
  sc_signal<uint32_t> xdmaChannel_c2hDescByp_len;
  sc_signal<sc_bv<DMA_ADDR_WIDTH>> xdmaChannel_c2hDescByp_dst_addr;
  sc_signal<uint32_t> xdmaChannel_c2hDescByp_ctl;
  sc_signal<bool> xdmaChannel_c2hDescByp_ready;
  sc_signal<bool> xdmaChannel_c2hDescByp_desc_done;

  sc_signal<sc_bv<DMA_DATA_WIDTH_IN_BYTES>> xdmaChannel_rawH2cAxiStream_tstrb;
  sc_signal<bool> xdmaChannel_rawH2cAxiStream_tlast;
  sc_signal<bool> xdmaChannel_rawH2cAxiStream_tuser;
  sc_signal<sc_bv<DMA_DATA_WIDTH_IN_BYTES>> xdmaChannel_rawH2cAxiStream_tkeep;
  sc_signal<bool> xdmaChannel_rawH2cAxiStream_tready;
  sc_signal<bool> xdmaChannel_rawH2cAxiStream_tvalid;
  sc_signal<sc_bv<DMA_DATA_WIDTH>> xdmaChannel_rawH2cAxiStream_tdata;

  sc_signal<sc_bv<DMA_DATA_WIDTH_IN_BYTES>> xdmaChannel_rawC2hAxiStream_tstrb;
  sc_signal<bool> xdmaChannel_rawC2hAxiStream_tlast;
  sc_signal<bool> xdmaChannel_rawC2hAxiStream_tuser;
  sc_signal<sc_bv<DMA_DATA_WIDTH_IN_BYTES>> xdmaChannel_rawC2hAxiStream_tkeep;
  sc_signal<bool> xdmaChannel_rawC2hAxiStream_tready;
  sc_signal<bool> xdmaChannel_rawC2hAxiStream_tvalid;
  sc_signal<sc_bv<DMA_DATA_WIDTH>> xdmaChannel_rawC2hAxiStream_tdata;

  template <typename T>
  void connect_user_logic(T* dev) {
    dev->xdmaChannel_h2cDescByp_load(xdmaChannel_h2cDescByp_load);
    dev->xdmaChannel_h2cDescByp_src_addr(xdmaChannel_h2cDescByp_src_addr);
    dev->xdmaChannel_h2cDescByp_len(xdmaChannel_h2cDescByp_len);
    dev->xdmaChannel_h2cDescByp_dst_addr(xdmaChannel_h2cDescByp_dst_addr);
    dev->xdmaChannel_h2cDescByp_ctl(xdmaChannel_h2cDescByp_ctl);
    dev->xdmaChannel_h2cDescByp_ready(xdmaChannel_h2cDescByp_ready);
    dev->xdmaChannel_h2cDescByp_desc_done(xdmaChannel_h2cDescByp_desc_done);
    dev->xdmaChannel_c2hDescByp_load(xdmaChannel_c2hDescByp_load);
    dev->xdmaChannel_c2hDescByp_src_addr(xdmaChannel_c2hDescByp_src_addr);
    dev->xdmaChannel_c2hDescByp_len(xdmaChannel_c2hDescByp_len);
    dev->xdmaChannel_c2hDescByp_dst_addr(xdmaChannel_c2hDescByp_dst_addr);
    dev->xdmaChannel_c2hDescByp_ctl(xdmaChannel_c2hDescByp_ctl);
    dev->xdmaChannel_c2hDescByp_ready(xdmaChannel_c2hDescByp_ready);
    dev->xdmaChannel_c2hDescByp_desc_done(xdmaChannel_c2hDescByp_desc_done);
    dev->xdmaChannel_rawH2cAxiStream_tlast(xdmaChannel_rawH2cAxiStream_tlast);
    dev->xdmaChannel_rawH2cAxiStream_tkeep(xdmaChannel_rawH2cAxiStream_tkeep);
    dev->xdmaChannel_rawH2cAxiStream_tready(xdmaChannel_rawH2cAxiStream_tready);
    dev->xdmaChannel_rawH2cAxiStream_tvalid(xdmaChannel_rawH2cAxiStream_tvalid);
    dev->xdmaChannel_rawH2cAxiStream_tdata(xdmaChannel_rawH2cAxiStream_tdata);
    dev->xdmaChannel_rawC2hAxiStream_tlast(xdmaChannel_rawC2hAxiStream_tlast);
    dev->xdmaChannel_rawC2hAxiStream_tkeep(xdmaChannel_rawC2hAxiStream_tkeep);
    dev->xdmaChannel_rawC2hAxiStream_tready(xdmaChannel_rawC2hAxiStream_tready);
    dev->xdmaChannel_rawC2hAxiStream_tvalid(xdmaChannel_rawC2hAxiStream_tvalid);
    dev->xdmaChannel_rawC2hAxiStream_tdata(xdmaChannel_rawC2hAxiStream_tdata);
  }
  template <typename T>
  void connect_xdma(T* dev) {
    dev->descriptor_bypass_channels[0].dsc_bypass_bridge_h2c.load(
        xdmaChannel_h2cDescByp_load);
    dev->descriptor_bypass_channels[0].dsc_bypass_bridge_h2c.src_addr(
        xdmaChannel_h2cDescByp_src_addr);
    dev->descriptor_bypass_channels[0].dsc_bypass_bridge_h2c.len(
        xdmaChannel_h2cDescByp_len);
    dev->descriptor_bypass_channels[0].dsc_bypass_bridge_h2c.dst_addr(
        xdmaChannel_h2cDescByp_dst_addr);
    dev->descriptor_bypass_channels[0].dsc_bypass_bridge_h2c.ctl(
        xdmaChannel_h2cDescByp_ctl);
    dev->descriptor_bypass_channels[0].dsc_bypass_bridge_h2c.ready(
        xdmaChannel_h2cDescByp_ready);
    dev->descriptor_bypass_channels[0].dsc_bypass_bridge_c2h.load(
        xdmaChannel_c2hDescByp_load);
    dev->descriptor_bypass_channels[0].dsc_bypass_bridge_c2h.src_addr(
        xdmaChannel_c2hDescByp_src_addr);
    dev->descriptor_bypass_channels[0].dsc_bypass_bridge_c2h.len(
        xdmaChannel_c2hDescByp_len);
    dev->descriptor_bypass_channels[0].dsc_bypass_bridge_c2h.dst_addr(
        xdmaChannel_c2hDescByp_dst_addr);
    dev->descriptor_bypass_channels[0].dsc_bypass_bridge_c2h.ctl(
        xdmaChannel_c2hDescByp_ctl);
    dev->descriptor_bypass_channels[0].dsc_bypass_bridge_c2h.ready(
        xdmaChannel_c2hDescByp_ready);
    dev->descriptor_bypass_channels[0].h2c_bridge.tlast(
        xdmaChannel_rawH2cAxiStream_tlast);
    dev->descriptor_bypass_channels[0].h2c_bridge.tuser(
        xdmaChannel_rawH2cAxiStream_tuser);
    dev->descriptor_bypass_channels[0].h2c_bridge.tstrb(
        xdmaChannel_rawH2cAxiStream_tkeep);
    dev->descriptor_bypass_channels[0].h2c_bridge.tready(
        xdmaChannel_rawH2cAxiStream_tready);
    dev->descriptor_bypass_channels[0].h2c_bridge.tvalid(
        xdmaChannel_rawH2cAxiStream_tvalid);
    dev->descriptor_bypass_channels[0].h2c_bridge.tdata(
        xdmaChannel_rawH2cAxiStream_tdata);
    dev->descriptor_bypass_channels[0].c2h_bridge.tlast(
        xdmaChannel_rawC2hAxiStream_tlast);
    dev->descriptor_bypass_channels[0].c2h_bridge.tuser(
        xdmaChannel_rawC2hAxiStream_tuser);
    dev->descriptor_bypass_channels[0].c2h_bridge.tstrb(
        xdmaChannel_rawC2hAxiStream_tkeep);
    dev->descriptor_bypass_channels[0].c2h_bridge.tready(
        xdmaChannel_rawC2hAxiStream_tready);
    dev->descriptor_bypass_channels[0].c2h_bridge.tvalid(
        xdmaChannel_rawC2hAxiStream_tvalid);
    dev->descriptor_bypass_channels[0].c2h_bridge.tdata(
        xdmaChannel_rawC2hAxiStream_tdata);
  }
  template <typename T>
  void connect_user_logic(T& dev) {
    connect_user_logic(&dev);
  }
  template <typename T>
  void connect_xdma(T& dev) {
    connect_xdma(&dev);
  }
  xdma_bypass_signal(sc_core::sc_module_name name)
      : sc_module(name),
        xdmaChannel_h2cDescByp_load("xdmaChannel_h2cDescByp_load"),
        xdmaChannel_h2cDescByp_src_addr("xdmaChannel_h2cDescByp_src_addr"),
        xdmaChannel_h2cDescByp_len("xdmaChannel_h2cDescByp_len"),
        xdmaChannel_h2cDescByp_dst_addr("xdmaChannel_h2cDescByp_dst_addr"),
        xdmaChannel_h2cDescByp_ctl("xdmaChannel_h2cDescByp_ctl"),
        xdmaChannel_h2cDescByp_ready("xdmaChannel_h2cDescByp_ready"),
        xdmaChannel_h2cDescByp_desc_done("xdmaChannel_h2cDescByp_desc_done"),
        xdmaChannel_c2hDescByp_load("xdmaChannel_c2hDescByp_load"),
        xdmaChannel_c2hDescByp_src_addr("xdmaChannel_c2hDescByp_src_addr"),
        xdmaChannel_c2hDescByp_len("xdmaChannel_c2hDescByp_len"),
        xdmaChannel_c2hDescByp_dst_addr("xdmaChannel_c2hDescByp_dst_addr"),
        xdmaChannel_c2hDescByp_ctl("xdmaChannel_c2hDescByp_ctl"),
        xdmaChannel_c2hDescByp_ready("xdmaChannel_c2hDescByp_ready"),
        xdmaChannel_c2hDescByp_desc_done("xdmaChannel_c2hDescByp_desc_done"),
        xdmaChannel_rawH2cAxiStream_tstrb("xdmaChannel_rawH2cAxiStream_tstrb"),
        xdmaChannel_rawH2cAxiStream_tlast("xdmaChannel_rawH2cAxiStream_tlast"),
        xdmaChannel_rawH2cAxiStream_tuser("xdmaChannel_rawH2cAxiStream_tuser"),
        xdmaChannel_rawH2cAxiStream_tkeep("xdmaChannel_rawH2cAxiStream_tkeep"),
        xdmaChannel_rawH2cAxiStream_tready(
            "xdmaChannel_rawH2cAxiStream_tready"),
        xdmaChannel_rawH2cAxiStream_tvalid(
            "xdmaChannel_rawH2cAxiStream_tvalid"),
        xdmaChannel_rawH2cAxiStream_tdata("xdmaChannel_rawH2cAxiStream_tdata"),
        xdmaChannel_rawC2hAxiStream_tstrb("xdmaChannel_rawC2hAxiStream_tstrb"),
        xdmaChannel_rawC2hAxiStream_tlast("xdmaChannel_rawC2hAxiStream_tlast"),
        xdmaChannel_rawC2hAxiStream_tuser("xdmaChannel_rawC2hAxiStream_tuser"),
        xdmaChannel_rawC2hAxiStream_tkeep("xdmaChannel_rawC2hAxiStream_tkeep"),
        xdmaChannel_rawC2hAxiStream_tready(
            "xdmaChannel_rawC2hAxiStream_tready"),
        xdmaChannel_rawC2hAxiStream_tvalid(
            "xdmaChannel_rawC2hAxiStream_tvalid"),
        xdmaChannel_rawC2hAxiStream_tdata("xdmaChannel_rawC2hAxiStream_tdata") {
  }
};

class xdma_signal : public sc_core::sc_module {
 public:
  xdma_bypass_signal bypass_channel;
  sc_signal<bool> axilRegBlock_awvalid;
  type_adapter<sc_bv<AXIL_AWPROT_WIDTH>, uint32_t,
               bvn_to_uint32t<AXIL_AWPROT_WIDTH>>
      axilRegBlock_awprot_typeAdpater;
  sc_signal<bool> axilRegBlock_awready;
  sc_signal<bool> axilRegBlock_wvalid;
  type_adapter<sc_bv<AXIL_WSTRB_WIDTH>, uint32_t,
               bvn_to_uint32t<AXIL_WSTRB_WIDTH>>
      axilRegBlock_wstrb_typeAdpater;
  sc_signal<bool> axilRegBlock_wready;
  sc_signal<bool> axilRegBlock_bvalid;
  type_adapter<uint32_t, sc_bv<AXIL_BRESP_WIDTH>,
               uint32t_to_bvn<AXIL_BRESP_WIDTH>>
      axilRegBlock_bresp_typeAdpater;
  sc_signal<bool> axilRegBlock_bready;
  sc_signal<bool> axilRegBlock_arvalid;
  type_adapter<sc_bv<AXIL_ARPROT_WIDTH>, uint32_t,
               bvn_to_uint32t<AXIL_ARPROT_WIDTH>>
      axilRegBlock_arprot_typeAdpater;
  sc_signal<bool> axilRegBlock_arready;
  sc_signal<bool> axilRegBlock_rvalid;
  type_adapter<uint32_t, sc_bv<AXIL_RRESP_WIDTH>,
               uint32t_to_bvn<AXIL_RRESP_WIDTH>>
      axilRegBlock_rresp_typeAdpater;
  sc_signal<bool> axilRegBlock_rready;
  type_adapter<sc_bv<BRIDGE_ADDR_WIDTH>, uint32_t,
               bvn_to_uint32t<BRIDGE_ADDR_WIDTH>>
      axilRegBlock_awaddr_typeAdpater;
  type_adapter<sc_bv<BRIDGE_ADDR_WIDTH>, uint32_t,
               bvn_to_uint32t<BRIDGE_ADDR_WIDTH>>
      axilRegBlock_araddr_typeAdpater;
  sc_signal<sc_bv<BRIDGE_DATA_WIDTH>> axilRegBlock_wdata;
  sc_signal<sc_bv<BRIDGE_DATA_WIDTH>> axilRegBlock_rdata;
  template <typename T>
  void connect_xdma(T& dev) {
    connect_xdma(&dev);
  }
  template <typename T>
  void connect_xdma(T* dev) {
    bypass_channel.connect_xdma(dev);
    dev->user_bar.awvalid(axilRegBlock_awvalid);
    dev->user_bar.awprot(axilRegBlock_awprot_typeAdpater.out_pin_signal);
    dev->user_bar.awready(axilRegBlock_awready);
    dev->user_bar.wvalid(axilRegBlock_wvalid);
    dev->user_bar.wstrb(axilRegBlock_wstrb_typeAdpater.out_pin_signal);
    dev->user_bar.wready(axilRegBlock_wready);
    dev->user_bar.bvalid(axilRegBlock_bvalid);
    dev->user_bar.bresp(axilRegBlock_bresp_typeAdpater.in_pin_signal);
    dev->user_bar.bready(axilRegBlock_bready);
    dev->user_bar.arvalid(axilRegBlock_arvalid);
    dev->user_bar.arprot(axilRegBlock_arprot_typeAdpater.out_pin_signal);
    dev->user_bar.arready(axilRegBlock_arready);
    dev->user_bar.rvalid(axilRegBlock_rvalid);
    dev->user_bar.rresp(axilRegBlock_rresp_typeAdpater.in_pin_signal);
    dev->user_bar.rready(axilRegBlock_rready);
    dev->user_bar.awaddr(axilRegBlock_awaddr_typeAdpater.out_pin_signal);
    dev->user_bar.araddr(axilRegBlock_araddr_typeAdpater.out_pin_signal);
    dev->user_bar.wdata(axilRegBlock_wdata);
    dev->user_bar.rdata(axilRegBlock_rdata);
  }
  template <typename T>
  void connect_user_logic(T& dev) {
    connect_user_logic(&dev);
  }
  template <typename T>
  void connect_user_logic(T* dev) {
    bypass_channel.connect_user_logic(dev);
    dev->axilRegBlock_awvalid(axilRegBlock_awvalid);
    dev->axilRegBlock_awprot(axilRegBlock_awprot_typeAdpater.in_pin_signal);
    dev->axilRegBlock_awready(axilRegBlock_awready);
    dev->axilRegBlock_wvalid(axilRegBlock_wvalid);
    dev->axilRegBlock_wstrb(axilRegBlock_wstrb_typeAdpater.in_pin_signal);
    dev->axilRegBlock_wready(axilRegBlock_wready);
    dev->axilRegBlock_bvalid(axilRegBlock_bvalid);
    dev->axilRegBlock_bresp(axilRegBlock_bresp_typeAdpater.out_pin_signal);
    dev->axilRegBlock_bready(axilRegBlock_bready);
    dev->axilRegBlock_arvalid(axilRegBlock_arvalid);
    dev->axilRegBlock_arprot(axilRegBlock_arprot_typeAdpater.in_pin_signal);
    dev->axilRegBlock_arready(axilRegBlock_arready);
    dev->axilRegBlock_rvalid(axilRegBlock_rvalid);
    dev->axilRegBlock_rresp(axilRegBlock_rresp_typeAdpater.out_pin_signal);
    dev->axilRegBlock_rready(axilRegBlock_rready);
    dev->axilRegBlock_awaddr(axilRegBlock_awaddr_typeAdpater.in_pin_signal);
    dev->axilRegBlock_araddr(axilRegBlock_araddr_typeAdpater.in_pin_signal);
    dev->axilRegBlock_wdata(axilRegBlock_wdata);
    dev->axilRegBlock_rdata(axilRegBlock_rdata);
  }
  xdma_signal(sc_core::sc_module_name name)
      : sc_module(name),
        bypass_channel("xdma-bypass-signal"),
        // clk("clk"),resetn("reset-n"),
        axilRegBlock_awvalid("axilRegBlock_awvalid"),
        axilRegBlock_awprot_typeAdpater("axilRegBlock_awprot"),
        axilRegBlock_awready("axilRegBlock_awready"),
        axilRegBlock_wvalid("axilRegBlock_wvalid"),
        axilRegBlock_wstrb_typeAdpater("axilRegBlock_wstrb"),
        axilRegBlock_wready("axilRegBlock_wready"),
        axilRegBlock_bvalid("axilRegBlock_bvalid"),
        axilRegBlock_bresp_typeAdpater("axilRegBlock_bresp"),
        axilRegBlock_bready("axilRegBlock_bready"),
        axilRegBlock_arvalid("axilRegBlock_arvalid"),
        axilRegBlock_arprot_typeAdpater("axilRegBlock_arprot"),
        axilRegBlock_arready("axilRegBlock_arready"),
        axilRegBlock_rvalid("axilRegBlock_rvalid"),
        axilRegBlock_rresp_typeAdpater("axilRegBlock_rresp"),
        axilRegBlock_rready("axilRegBlock_rready"),
        axilRegBlock_awaddr_typeAdpater("axilRegBlock_awaddr"),
        axilRegBlock_araddr_typeAdpater("axilRegBlock_araddr"),
        axilRegBlock_wdata("axilRegBlock_wdata"),
        axilRegBlock_rdata("axilRegBlock_rdata") {}
};

#endif