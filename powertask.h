/*
  A power aware task scheduling interface.
  
  LIMITATIONS:
    - NONE of this code is interrupt time or multithread reentrant.
  
  This is a C99 header file.
  
  CJ Emerson and Orion Lawlor, 2021-01, public domain
*/
#ifndef __UAF_POWERTASK_H
#define __UAF_POWERTASK_H

#include <stdint.h> /* for uint16_t and such */

/************** Telemetry Handling *******************/
/// A powertask_ID_t identifies a task in telemetry and logs.
///  It's traditionally printed in hex, so you can pick a number like 0xB0F3.
/// ID below 0x1000 or above 0xF000 are reserved for the task system itself
///  and should not be used by applications.
typedef uint16_t powertask_ID_t;

/// One byte of telemetry data.
typedef unsigned char powertask_data_t;

/// Length of a telemetry data field.
typedef uint16_t powertask_length_t;

/// This is the data in a telemetry packet header
struct powertask_telemetry_header_t {
    powertask_ID_t ID; // ID of the task
    powertask_length_t length; // number of bytes listed below
}; 
typedef struct powertask_telemetry_header_t powertask_telemetry_header_t;

/// This is a telemetry packet: in-memory or on-the-wire format
///    for getting telemetry data into or back from a task.
/// Typically you will take your data in some format, so you will
///    write your own struct here and typecast.
struct powertask_telemetry_t {
    powertask_telemetry_header_t header; 
    powertask_data_t data[]; // variable-length array
};
typedef struct powertask_telemetry_t powertask_telemetry_t;


/*********** Task Handling **************/
/// This is a short human-readable string used to identify a task.
typedef const char *powertask_name_t;

/// This is the type of a result code returned from a task function.
typedef uint16_t powertask_result_t;

/// These are possible result codes:
#define POWERTASK_RESULT_OK 0x1001  /* task completed successfully and produced output */
#define POWERTASK_RESULT_RETRY 0x10FF /* task did not complete and must be automatically retried when possible */
#define POWERTASK_RESULT_FIRST 0x1000 /* result codes less than this are invalid */
#define POWERTASK_RESULT_FAILURE  0x2000 /* result codes >= this are failure */
#define POWERTASK_RESULT_FAIL_QUIET 0x2000 /* task failed, no output produced, 12-bit reason code is added to this */
#define POWERTASK_RESULT_FAIL_OUTPUT 0x4000 /* task failed, some output produced, 12-bit reason code is added to this */
#define POWERTASK_RESULT_LAST 0x6000 /* result codes bigger than this are invalid */

/// This is a user-written function that actually performs a task.
///   input is the incoming telemetry data.
///   output is the outgoing telemetry data.
typedef powertask_result_t (*powertask_function_t)(
    const powertask_telemetry_t *input,
    powertask_telemetry_t *output);


/// An powertask_energy_t is an amount of battery energy required for this task.
///  Units are in joules.
typedef uint16_t powertask_energy_t; 


/// This struct describes the constant attributes of a task.
///   It is separate from the runtime attributes so that 
//    this struct can be declared "const static" and be stored in constant memory.
struct powertask_attribute_t {
    powertask_ID_t ID; // ID of this task
    powertask_name_t name; // short human-readable name of the task (for debugging)
    powertask_energy_t minimum_battery; // the battery must have this much energy (Joules) before we should begin this task
    powertask_function_t function; // function that executes the task
    powertask_length_t input_length; // bytes of input required from telemetry
    powertask_length_t output_length; // bytes of output produced for telemetry
};
typedef struct powertask_attribute_t powertask_attribute_t;

/// Register a new task with the powertask system.
///  The attribute must be declared const static and cannot be changed during runtime.
/// This function is normally called at startup before running any tasks, such as from main or an __attribute__((constructor)); function.
void powertask_register(const powertask_attribute_t *attribute);


/*********** Advanced / system level interface *************/

/// This struct describes a task at runtime.  Callers can allocate this,
///  so that the telemetry and task system does not 
//   need to do dynamic memory allocation.
struct powertask_task_t {
    /// Constant data about the task, like its attribute->ID
    const powertask_attribute_t *attribute; 
    
    /// Telemetry data for this task gets stored here.
    ///   Callers allocate these pointers, typically to static buffers.
    ///   If set to NULL, before running they are dynamically allocated as:
    //      calloc(1,sizeof(powertask_telemetry_header_t)+task->attribute->input_length);
    powertask_telemetry_t *input;
    powertask_telemetry_t *output;
    
    /// This is the search tree for all registered tasks.
    ///   "lower" has smaller attribute->ID than us.
    ///   "higher" has larger attribute->ID than us.
    /// You can improve tree balance by your registration order.
    struct powertask_task_t *lower,*higher;
    
    /// This is a doubly-linked list of currently runnable tasks.
    /// If this task is not runnable, these pointers are set to NULL.
    /// The task list always contains the idle task (and usually a watchdog too),
    ///   so this list is never empty.
    struct powertask_task_t *prev,*next;
};
typedef struct powertask_task_t powertask_task_t;

/// This advanced function registers a new task with the powertask system.
///  The caller must have allocated both pointers static, so they never go away.
///  It avoids dynamic allocation completely if you preallocate telemetry input and output.
void powertask_task_register(const powertask_attribute_t *attribute,powertask_task_t *task);

/// Look up the runtime task structure for this task ID.
///  Returns 0 if that task ID is not registered.
powertask_task_t *powertask_task_lookup(powertask_ID_t ID);

/// Make this task runnable--the task is added to the runnable queue.
///  If this task requires input, you must fill out the data portion 
///   of the returned telemetry structure. 
powertask_telemetry_t *powertask_make_runnable(powertask_ID_t ID);

/// Run the next task.  Returns 1 if tasks still exist to run.
int powertask_run_next(void);

/// Set the debugging verbosity level.  0 == no debug prints.  Higher numbers == more prints.
void powertask_debug(int debug_level);


#endif


