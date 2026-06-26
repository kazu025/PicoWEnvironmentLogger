#pragma once

extern volatile bool g_flash_maintenance_busy;
inline bool isFlashMaintenanceBusy(){
    return g_flash_maintenance_busy;
}
