#include "prx.h"
#include "openps3ftp.h"
#include "test.h"

SYS_MODULE_START(prx_start);
SYS_MODULE_STOP(prx_stop);
SYS_MODULE_EXIT(prx_exit);
SYS_MODULE_INFO(FTPD_TEST, 0, 4, 2);

sys_ppu_thread_t prx_tid;
bool prx_running = false;

inline void _sys_ppu_thread_exit(uint64_t val)
{
	system_call_1(41, val);
}

inline sys_prx_id_t prx_get_module_id_by_name(const char* name)
{
	system_call_3(496, (uint64_t)(uintptr_t) name, 0, NULL);
	return (sys_prx_id_t) p1;
}

inline sys_prx_id_t prx_get_module_id_by_address(void* addr)
{
	system_call_1(461, (uint64_t)(uintptr_t) addr);
	return (sys_prx_id_t) p1;
}

void finalize_module(void)
{
	uint64_t meminfo[5];

	sys_prx_id_t prx = prx_get_module_id_by_address(finalize_module);

	meminfo[0] = 0x28;
	meminfo[1] = 2;
	meminfo[3] = 0;

	system_call_3(482, prx, 0, (uintptr_t) meminfo);
}

void prx_unload(void)
{
	sys_prx_id_t prx = prx_get_module_id_by_address(prx_unload);
	
	system_call_3(483, prx, 0, NULL);
}

int prx_stop(void)
{
	if(prx_running)
	{
		prx_running = false;

		uint64_t prx_exitcode;
		sys_ppu_thread_join(prx_tid, &prx_exitcode);
	}

	finalize_module();
	_sys_ppu_thread_exit(0);
	return SYS_PRX_STOP_OK;
}

int prx_exit(void)
{
	if(prx_running)
	{
		prx_running = false;

		uint64_t prx_exitcode;
		sys_ppu_thread_join(prx_tid, &prx_exitcode);
	}

	prx_unload();
	_sys_ppu_thread_exit(0);
	return SYS_PRX_STOP_OK;
}

void prx_main(uint64_t ptr)
{
	prx_running = true;

	// wait for ftpd
	do {
		sys_timer_sleep(1);
	} while(prx_get_module_id_by_name("FTPD") <= 0);

	// add ftp commands using api
	prx_command_register("TEST", cmd_test);

	while(prx_running)
	{
		sys_timer_sleep(1);
	}

	// unregister commands
	if(prx_get_module_id_by_name("FTPD") > 0)
	{
		prx_command_unregister(cmd_test);
	}
	
	sys_ppu_thread_exit(0);
}

int prx_start(size_t args, void* argv)
{
	sys_ppu_thread_create(&prx_tid, prx_main, 0, 1000, 0x1000, SYS_PPU_THREAD_CREATE_JOINABLE, (char*) "OpenPS3FTP-TEST");
	_sys_ppu_thread_exit(0);
	return SYS_PRX_RESIDENT;
}
