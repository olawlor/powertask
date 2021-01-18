# powertask
A power-aware task scheduler for embedded systems

Features:
    - Intelligently choose the next single foreground task to begin next.
        - Commanded from uplink telemetry
        - Respect each task's power limits
        - Logged back for downlink telemetry
    - Supports multiple background tasks (e.g., watchdog, telemetry, etc.)
    

Aspects:
    - Language support: prefer plain C.
    - Static vs dynamic memory allocation: prefer static allocation.
    - Lean on FreeRTOS for prioritization, semaphores, etc.

