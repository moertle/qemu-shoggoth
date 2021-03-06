/*
 * Rapid Analysis QEMU System Emulator
 *
 * Copyright (c) 2020 Cromulence LLC
 *
 * Distribution Statement A
 *
 * Approved for Public Release, Distribution Unlimited
 *
 * Authors:
 *  Joseph Walker
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 * 
 * The creation of this code was funded by the US Government.
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "plugin/ra_cb.h"
#include "plugin/cpu_cb.h"
#include "plugin/net_cb.h"
#include "plugin/vm_cb.h"
#include "plugin/plugin_mgr.h"
#include "migration/snapshot.h"
#include "cpu.h"
#include "exec/ram_addr.h"
#include "oshandler/oshandler.h"
#include "qapi/qmp/qpointer.h"
#include "sysemu/hw_accel.h"


void notify_ra_start(CommsWorkItem* work)
{
    // Loop through the plugins
    PluginInstanceList *p = NULL;
    QLIST_FOREACH(p, &plugin_instance_list, next)
    {
        // Check if the on_ra_start callback is set
        if (p->instance->cb.on_ra_start)
        {
            // Call the plugin callback
            p->instance->cb.on_ra_start(p->instance, work);
        }
    }
}

void notify_ra_stop(CPUState *cpu, SHA1_HASH_TYPE job_hash)
{
    // Collect the current state of rapid analysis 
    RSaveTree *rst = rapid_analysis_get_instance(cpu);

    // Loop through the plugins
    PluginInstanceList *p = NULL;
    QLIST_FOREACH(p, &plugin_instance_list, next)
    {
        // Variable
        CommsResultsItem *work_results = NULL;
        
        // Check if the on_ra_stop callback is set
        if (p->instance->cb.on_ra_stop)
        {
            // Initialize request to something meaningful
            JOB_REPORT_TYPE request = rst->job_report_mask;

            // Get the mask for the work result
            if (p->instance->cb.get_ra_report_type)
            {
                // Instead use the report requested by the plugin.
                request = p->instance->cb.get_ra_report_type(p->instance);
            }

            // request a report
            CommsMessage *result_message = build_rsave_report(rst, job_hash, request, NULL);

            if (result_message)
            {
                // Put together the work results
                work_results = g_new(CommsResultsItem, 1);

                // Set the message field in the work result.
                work_results->msg = result_message;   

                // Call the plugin callback
                p->instance->cb.on_ra_stop(p->instance, work_results);

                // Free the pointers (not RST though)
                if (result_message) g_free(result_message);
                if (work_results) g_free(work_results);                    
            }             
        }
    }    
}

void notify_ra_idle(void)
{
    // Loop through the plugins
    PluginInstanceList *p = NULL;
    QLIST_FOREACH(p, &plugin_instance_list, next)
    {
        // Check if the on_ra_stop callback is set
        if (p->instance->cb.on_ra_idle)
        {
            // Call the plugin callback
            p->instance->cb.on_ra_idle(p->instance);
        }
    }
}

void notify_exec_instruction(CPUState *cs, uint64_t vaddr)
{
    // Perform the translation from vaddr to paddr.
    hwaddr paddr = cpu_get_phys_page_debug(cs, vaddr);
    if (paddr == -1) {
        printf("notify_exec_instruction: No virtual translation for code address %lX!\n", vaddr);
        return;
    }

    // Get the pointer to executing code in host memory.
    void *code = qemu_map_ram_ptr_nofault(NULL, paddr + (~TARGET_PAGE_MASK & vaddr), NULL);
    if (!code) {
        printf("notify_exec_instruction: No host memory for code address %lX!\n", paddr);
        return;
    }

    PluginInstanceList *p = NULL;
    QLIST_FOREACH(p, &plugin_instance_list, next)
    {
        // Check if the callback is set
        if(p->instance->cb.on_execute_instruction)
        {
            // Call the plugin callback
            p->instance->cb.on_execute_instruction(p->instance, vaddr, code);
        }
    }
}

void notify_read_memory(CPUState *cs, uint64_t paddr, uint8_t *value, int size)
{
    // Get the pointer to executing code in host memory. Could fail...
    void *ram_ptr = qemu_map_ram_ptr_nofault(NULL, paddr, NULL);

    PluginInstanceList *p = NULL;
    QLIST_FOREACH(p, &plugin_instance_list, next)
    {
        // Check if the callback is set
        if (p->instance->cb.on_memory_read)
        {
            // Call the plugin callback
            p->instance->cb.on_memory_read(p->instance, paddr, value, ram_ptr, size);
        }
    }
}

void notify_write_memory(CPUState *cs, uint64_t paddr, const uint8_t *value, int size)
{
   // Get the pointer to executing code in host memory. Could fail...
   void *ram_ptr = qemu_map_ram_ptr_nofault(NULL, paddr, NULL);

   PluginInstanceList *p = NULL;
   QLIST_FOREACH(p, &plugin_instance_list, next)
   {
        // Check if the callback is set
        if (p->instance->cb.on_memory_write)
        {
            // Call the plugin callback
            p->instance->cb.on_memory_write(p->instance, paddr, value, ram_ptr, size);
        }
   }
}

void notify_breakpoint_hit(CPUState *cs, OSBreakpoint* bp)
{
    CPUClass *cpu_class = CPU_GET_CLASS(cs);
    cpu_synchronize_state(cs);

    // Loop through the plugins
    PluginInstanceList *p = NULL;
    QLIST_FOREACH(p, &plugin_instance_list, next)
    {
        // Check if the callback is set
        if (p->instance->cb.on_breakpoint_hit)
        {
            vaddr pc = cpu_class->get_pc(cs);

            // Call the plugin callback
            p->instance->cb.on_breakpoint_hit(p->instance, cs->cpu_index, pc, bp->id);
        }
    }    
}

void notify_exception(int32_t exception)
{
    // Loop through the plugins
    PluginInstanceList *p = NULL;
    QLIST_FOREACH(p, &plugin_instance_list, next)
    {
        // Check if the callback is set
        if (p->instance->cb.on_exception)
        {
            // Call the plugin callback
            p->instance->cb.on_exception(p->instance, exception);
        }
    } 
}

void notify_syscall(uint64_t number, ...)
{
    va_list valist;
    va_start(valist, number);

    // Loop through the plugins
    PluginInstanceList *p = NULL;
    QLIST_FOREACH(p, &plugin_instance_list, next)
    {
        // Check if the callback is set
        if (p->instance->cb.on_syscall)
        {
            // Call the plugin callback
            p->instance->cb.on_syscall(p->instance, number, valist);
        }
    } 

    va_end(valist); 
}

void notify_interrupt(int mask)
{
    // Loop through the plugins
    PluginInstanceList *p = NULL;
    QLIST_FOREACH(p, &plugin_instance_list, next)
    {
        // Check if the callback is set
        if (p->instance->cb.on_interrupt)
        {
            // Call the plugin callback
            p->instance->cb.on_interrupt(p->instance, mask);
        }
    } 
}

void notify_receving_packet(uint8_t **pkt_buf, uint32_t *pkt_size)
{
    // Loop through the plugins
    PluginInstanceList *p = NULL;
    QLIST_FOREACH(p, &plugin_instance_list, next)
    {
        // Check if the on_packet_recv callback is set
        if (p->instance->cb.on_packet_recv)
        {
            // Call the plugin callback
            p->instance->cb.on_packet_recv(p->instance, pkt_buf, pkt_size);
        }
    }
}

void notify_sending_packet(uint8_t **pkt_buf, uint32_t *pkt_size)
{
    // Loop through the plugins
    PluginInstanceList *p = NULL;
    QLIST_FOREACH(p, &plugin_instance_list, next)
    {
        // Check if the on_packet_send callback is set
        if (p->instance->cb.on_packet_send)
        {
            // Call the plugin callback
            p->instance->cb.on_packet_send(p->instance, pkt_buf, pkt_size);
        }
    } 
}

void notify_vm_shutdown(void)
{
    // Loop through the plugins
    PluginInstanceList *p = NULL;
    QLIST_FOREACH(p, &plugin_instance_list, next)
    {
        // Check if the on_packet_send callback is set
        if (p->instance->cb.on_vm_shutdown)
        {
            // Call the plugin callback
            p->instance->cb.on_vm_shutdown(p->instance);                
        }
    }
}
