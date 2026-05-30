// SPDX-License-Identifier: GPL-3.0+

#include <pcap/pcap.h>

#include <cstdio>

static char s_android_pcap_error[PCAP_ERRBUF_SIZE] =
	"Android app sandbox does not expose raw PCAP capture devices";

static void android_pcap_copy_error(char* errbuf)
{
	if (!errbuf)
		return;

	std::snprintf(errbuf, PCAP_ERRBUF_SIZE, "%s", s_android_pcap_error);
}

extern "C" pcap_t* pcap_open_live(const char* device, int snaplen, int promisc, int to_ms, char* errbuf)
{
	(void)device;
	(void)snaplen;
	(void)promisc;
	(void)to_ms;
	android_pcap_copy_error(errbuf);
	return nullptr;
}

extern "C" void pcap_close(pcap_t* handle)
{
	(void)handle;
}

extern "C" int pcap_next_ex(pcap_t* handle, struct pcap_pkthdr** header, const u_char** data)
{
	(void)handle;
	if (header)
		*header = nullptr;
	if (data)
		*data = nullptr;
	return -1;
}

extern "C" int pcap_sendpacket(pcap_t* handle, const u_char* buffer, int size)
{
	(void)handle;
	(void)buffer;
	(void)size;
	return -1;
}

extern "C" int pcap_findalldevs(pcap_if_t** alldevs, char* errbuf)
{
	if (alldevs)
		*alldevs = nullptr;
	android_pcap_copy_error(errbuf);
	return -1;
}

extern "C" void pcap_freealldevs(pcap_if_t* alldevs)
{
	(void)alldevs;
}

extern "C" int pcap_setnonblock(pcap_t* handle, int nonblock, char* errbuf)
{
	(void)handle;
	(void)nonblock;
	android_pcap_copy_error(errbuf);
	return -1;
}

extern "C" char* pcap_geterr(pcap_t* handle)
{
	(void)handle;
	return s_android_pcap_error;
}

extern "C" int pcap_datalink(pcap_t* handle)
{
	(void)handle;
	return DLT_EN10MB;
}

extern "C" const char* pcap_datalink_val_to_name(int dlt)
{
	return (dlt == DLT_EN10MB) ? "EN10MB" : "UNKNOWN";
}

extern "C" int pcap_compile(
	pcap_t* handle, struct bpf_program* program, const char* filter, int optimize, bpf_u_int32 netmask)
{
	(void)handle;
	(void)filter;
	(void)optimize;
	(void)netmask;
	if (program)
	{
		program->bf_len = 0;
		program->bf_insns = nullptr;
	}
	return -1;
}

extern "C" int pcap_setfilter(pcap_t* handle, struct bpf_program* program)
{
	(void)handle;
	(void)program;
	return -1;
}

extern "C" void pcap_freecode(struct bpf_program* program)
{
	if (!program)
		return;

	program->bf_len = 0;
	program->bf_insns = nullptr;
}
