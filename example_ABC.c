/**
 Simple example of powertask task registration.
*/
#include "powertask.h"


powertask_result_t function_A(const powertask_telemetry_t *input,
    powertask_telemetry_t *output)
{
    output->data[0]='A';
    powertask_telemetry_t *Bin=powertask_make_runnable(0xB007);
    Bin->data[0]='B';
   // powertask_make_runnable(0xCCC);
    return POWERTASK_RESULT_OK;
}
const static powertask_attribute_t attributes_A={
    0xA123, /* our task ID */
    "Demo A", /* human-readable name */
    1000, /* minimum battery energy (Joules) */
    function_A, /* function to run */
    0,/* bytes of telemetry input data required */
    1 /* bytes of telemetry output data produced */
};




powertask_result_t function_B(const powertask_telemetry_t *input,
    powertask_telemetry_t *output)
{
    char in=input->data[0];
    if (output->data[0]==0) 
    {
        output->data[0]=in;
        return POWERTASK_RESULT_RETRY; // we're not done yet
    }
    else 
    {
        output->data[0]++;
        return POWERTASK_RESULT_OK;
    }
}
const static powertask_attribute_t attributes_B={
    0xB007, /* our task ID */
    "Demo B", /* human-readable name */
    10000, /* minimum battery energy (Joules) */
    function_B, /* function to run */
    1,/* bytes of telemetry input data required */
    1 /* bytes of telemetry output data produced */
};



    

int main() 
{
    powertask_debug(9000); // debugging verbosity level
    
    powertask_register(&attributes_A);
    powertask_register(&attributes_B);
    //powertask_register(&attributes_C);
    
    powertask_make_runnable(0xA123);
    while (powertask_run_next()) {}
}

