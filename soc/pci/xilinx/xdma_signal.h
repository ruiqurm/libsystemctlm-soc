#ifndef XDMA_SIGNAL_H__
#define XDMA_SIGNAL_H__

#include "sysc/communication/sc_signal.h"
#include "sysc/kernel/sc_module.h"
#include "systemc.h"
using namespace sc_core;
class xdma_bypass_signal : public sc_core::sc_module
{
public:
	/* Write data channel.  */
	sc_signal<bool> dsc_byp_load;
	sc_signal<sc_bv<64>> dsc_byp_src_addr;
	sc_signal<sc_bv<28>> dsc_byp_len;
	sc_signal<sc_bv<64>> dsc_byp_dst_addr;
	sc_signal<sc_bv<16>> dsc_byp_ctl;
	sc_signal<bool> dsc_byp_ready;

	template<typename T>
	void connect(T *dev)
	{
		dev->dsc_byp_load(dsc_byp_load);
		dev->dsc_byp_src_addr(dsc_byp_src_addr);
		dev->dsc_byp_len(dsc_byp_len);
		dev->dsc_byp_dst_addr(dsc_byp_dst_addr);
		dev->dsc_byp_ctl(dsc_byp_ctl);
		dev->dsc_byp_ready(dsc_byp_ready);
	}

	void Trace(sc_trace_file *f)
	{
		/* Write data channel.  */
		sc_trace(f, dsc_byp_load, dsc_byp_load.name());
		sc_trace(f, dsc_byp_src_addr, dsc_byp_src_addr.name());
		sc_trace(f, dsc_byp_len, dsc_byp_len.name());
		sc_trace(f, dsc_byp_dst_addr, dsc_byp_dst_addr.name());
		sc_trace(f, dsc_byp_ctl, dsc_byp_ctl.name());
		sc_trace(f, dsc_byp_ready, dsc_byp_ready.name());
	}

	template<typename T>
	void connect(T& dev)
	{
		connect(&dev);
	}

	xdma_bypass_signal(sc_core::sc_module_name name) :
		sc_module(name),

		dsc_byp_load("dsc_byp_load"),
		dsc_byp_src_addr("dsc_byp_src_addr"),
		dsc_byp_len("dsc_byp_len"),
		dsc_byp_dst_addr("dsc_byp_dst_addr"),
		dsc_byp_ctl("dsc_byp_ctl"),
		dsc_byp_ready("dsc_byp_ready")
	{}

private:
};

#endif