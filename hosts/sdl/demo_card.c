#include "demo_card.h"
#include "embedded_demo_project.h"

m8_fat_result m8_desktop_demo_create(m8_fat_card **card,m8_fat_error *error) {
    m8_fat_result result=m8_fat_create(16u*1024*1024,card,error);
    if(result!=M8_FAT_OK)return result;
    result=m8_fat_write(*card,"Songs/DEMO1.M8S",m8_embedded_demo_project,
                        sizeof m8_embedded_demo_project,(m8_fat_time){0},0);
    if(result!=M8_FAT_OK) {
        if(error)*error=*m8_fat_last_error(*card);
        m8_fat_close(*card);*card=NULL;
    }
    return result;
}
