/**
 Functions built into the powertask system:
   implements the interface in powertask.h.
 
 CJ Emerson and Orion Lawlor, 2021-01, public domain
*/
#include <stdio.h>
#include <stdlib.h>
#include "powertask.h"

/// Debug support
static int powertask_debug_level=0;
void powertask_debug(int debug_level) {
    powertask_debug_level=debug_level; 
}
#define DEBUGF(level,params) { if (powertask_debug_level>=level)  printf params; }

void powertask_fatal(const char *why,int ID) 
{
    DEBUGF(0,("FATAL powertask ERROR: %s [%04x]\n",why,ID));
    exit(1);
}

/// This is the tree of all registered tasks.
static powertask_task_t *registered_tasks=0;

/// This is the current entry in the doubly linked list of all runnable tasks.
static powertask_task_t *runnable_tasks=0;

// Link this new task into the registered-tasks binary tree
static void powertask_link_into_tree(powertask_task_t *parent,powertask_task_t *task)
{      
    while (1) {
        if (parent->attribute->ID < task->attribute->ID)
        {
            if (parent->lower) parent=parent->lower;
            else { // we are their new lower leaf
                parent->lower=task;
                break;
            }
        }
        else if (parent->attribute->ID > task->attribute->ID)
        {
            if (parent->higher) parent=parent->higher;
            else { // we are their new higher leaf
                parent->higher=task;
                break;
            }
        }
        else /* wait, parent ID == attribute ID ?! */
        {
            DEBUGF(0,("powertask_register ID collision: old ID %04x (%s, %p), new ID %04x (%s,%p)\n",
                (int)parent->attribute->ID, parent->attribute->name, parent->attribute,
                (int)task->attribute->ID, task->attribute->name, task->attribute));
            powertask_fatal("powertask_register ID collision",task->attribute->ID);
        }
    }
}



/// This is the builtin idle task
#define powertask_ID_builtin_idle 0xFFFF
static powertask_result_t powertask_idle_task(const powertask_telemetry_t *input,
    powertask_telemetry_t *output)
{
    // microprocessor sleep mode here?
    DEBUGF(5,("idle\n"));
    return POWERTASK_RESULT_RETRY;
}
const static powertask_attribute_t attributes_idle_task={
    powertask_ID_builtin_idle, /* our task ID */
    "IdleTask", /* human-readable name */
    0, /* minimum battery energy (Joules) */
    powertask_idle_task, /* function to run */
    0, /* bytes of telemetry input data required */
    0 /* bytes of telemetry output data produced */
};

static void powertask_setup()
{
    // Register our builtin idle task
    powertask_register(&attributes_idle_task);
    powertask_make_runnable(powertask_ID_builtin_idle);    
    
    // Register any other utility tasks (mem read?  log read?)
}

void powertask_register(const powertask_attribute_t *attribute)
{
    // Allocate a clean blank task struct.
    //  We use calloc because it zeros the memory it allocates.
    powertask_task_t *task = (powertask_task_t*)calloc(1,sizeof(powertask_task_t));
    powertask_task_register(attribute,task);
}


void powertask_task_register(const powertask_attribute_t *attribute,powertask_task_t *task)
{
    DEBUGF(10,("powertask_register ID %04x (%s), %d battery, %d bytes in, %d bytes out\n",
        (int)attribute->ID, attribute->name,
        (int)attribute->minimum_battery, 
        (int)attribute->input_length,
        (int)attribute->output_length));
    
    // Link in the basics from the task struct:
    task->attribute = attribute;
    task->lower=task->higher=0;
    task->prev=task->next=0;
    
    if (registered_tasks==0) 
    { // This is the first registration ever.
        registered_tasks=task; // root of the search tree

        // This is our chance to register builtin tasks and such
        powertask_setup();
    }
    else 
    {
        powertask_link_into_tree(registered_tasks,task);
    }
}

/// Look up the runtime task structure for this task ID.
///  Returns 0 if that task ID is not registered.
powertask_task_t *powertask_task_lookup(powertask_ID_t ID)
{
    powertask_task_t *parent=registered_tasks;        
    while (1) {
        if (parent->attribute->ID < ID)
        {
            if (parent->lower) parent=parent->lower;
            else return 0; // hit leaf
        }
        else if (parent->attribute->ID > ID)
        {
            if (parent->higher) parent=parent->higher;
            else return 0; // hit leaf
        }
        else /* found it! */
        {
            return parent;
        }
    }
}

/// Allocate telemetry object (only called once per task, cached in task struct)
powertask_telemetry_t *powertask_allocate_telemetry(powertask_length_t len)
{
    DEBUGF(8,("  allocating %d bytes of telemetry\n",(int)len));
    powertask_telemetry_t *tel=(powertask_telemetry_t *)calloc(1,
        sizeof(powertask_telemetry_header_t)+len);
    return tel;
}

/// Make this task runnable--the task is added to the runnable queue.
///  If this task requires input, you must fill out the data portion 
///   of the returned telemetry structure. 
powertask_telemetry_t *powertask_make_runnable(powertask_ID_t ID)
{
    powertask_task_t *task=powertask_task_lookup(ID);
    if (task==0) powertask_fatal("Invalid task in powertask_make_runnable",ID);
    DEBUGF(3,("powertask_make_runnable %04x (%s)\n",(int)ID,task->attribute->name));
    
    // Is it already runnable?
    if (task->prev!=0) {
        DEBUGF(2,("  ignoring request to make task %04x runnable, already runnable",(int)ID));
        return task->input; //<- could this cause disaster?  fatal instead?
    }
    
    // Allocate telemetry slots (will be needed when it runs)
    if (task->input==0) 
        task->input=powertask_allocate_telemetry(task->attribute->input_length);
    if (task->output==0) 
        task->output=powertask_allocate_telemetry(task->attribute->output_length);
    
    // Link into doubly linked list of runnable tasks
    if (runnable_tasks==0) 
    { // first time running any task!
        task->prev=task;
        task->next=task;
        runnable_tasks=task;
    }
    else 
    { // link into existing list of runnable tasks
        task->next=runnable_tasks->next;
        runnable_tasks->next=task;
        task->prev=runnable_tasks;
        runnable_tasks=task; // cut into line?
    }
    
    // Rely on caller to fill in telemetry data (is this right?)
    return task->input;
}

powertask_energy_t powertask_current_battery=30000;  // <- FIXME: need a real battery interface

// Remove the current task from the runnable_tasks list,
//  and point to the previous task.
void remove_task(powertask_task_t *task)
{
    DEBUGF(3,("  removing %04x (%s) from the run queue\n",
        (int)task->attribute->ID,task->attribute->name));  
    task->prev->next=task->next;
    task->next->prev=task->prev;
    if (task==runnable_tasks)
        runnable_tasks=task->next;
}


/// Run the next task.  Returns 1 if tasks still exist to run.
int powertask_run_next(void)
{
    powertask_task_t *task=runnable_tasks;    
    powertask_energy_t need_battery=task->attribute->minimum_battery;

    DEBUGF(3,("run_next chooses %04x (%s)\n",
        (int)task->attribute->ID,task->attribute->name));
    if (powertask_current_battery >= need_battery)
    { // we have the energy to run this now
        powertask_result_t result;
        DEBUGF(3,("  running function %p\n",task->attribute->function));
        result=task->attribute->function(task->input,task->output);
        DEBUGF(3,("  function returns %04x\n",result));
        if (result==POWERTASK_RESULT_RETRY)
        {
            // Leave it in the runnable list, it will come around again
            
            // Move on to other tasks
            runnable_tasks=runnable_tasks->next;
        }
        else if (result==POWERTASK_RESULT_OK)
        {
            // FIXME: store output telemetry somewhere
            // It's successful, remove it from the runnable list
            remove_task(task);
        }
        else if (result>=POWERTASK_RESULT_FAIL_QUIET && result<POWERTASK_RESULT_FAIL_OUTPUT)
        {
            // Failed without output, remove it
            remove_task(task);
        }
        else if (result>=POWERTASK_RESULT_FAIL_OUTPUT && result<POWERTASK_RESULT_LAST)
        {
            // FIXME: store output telemetry somewhere
            // Failed with output, send it and remove it
            remove_task(task);
        }
        else // invalid result code
        {
            powertask_fatal("task returned invalid result code",result);
        }
    }
    else {
        DEBUGF(3,("  not enough battery, need %d have %d\n",
            (int)need_battery,(int)powertask_current_battery));   
        // Move on to other tasks
        runnable_tasks=runnable_tasks->next;
             
    }
    
    // We have nothing left to run
    return runnable_tasks!=runnable_tasks->next;
}






