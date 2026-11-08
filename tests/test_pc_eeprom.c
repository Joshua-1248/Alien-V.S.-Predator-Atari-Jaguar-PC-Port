#include "pc/pc_eeprom.h"
#include "eeprom.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char path[4096]={0};
    FILE *f;
    unsigned char bad[5]={0x91u,0x82u,0x73u,0x64u,0x55u};
    unsigned char check[5]={0};

    assert(avp_pc_eeprom_open_for_executable(NULL,path,sizeof(path)));
    assert(avp_pc_eeprom_read_word(0)==0xffffu);
    assert(avp_pc_eeprom_read_word(63)==0xffffu);
    avp_pc_eeprom_write_word(0,0x1234u);
    avp_pc_eeprom_write_word(63,0x5678u);
    avp_pc_eeprom_close();

    assert(avp_pc_eeprom_open_for_executable(NULL,NULL,0));
    assert(avp_pc_eeprom_read_word(0)==0x1234u);
    assert(avp_pc_eeprom_read_word(63)==0x5678u);
    avp_pc_eeprom_close();

    /* Malformed existing data must never be overwritten with 0xFF. */
    f=fopen(path,"wb");assert(f);
    assert(fwrite(bad,1,sizeof(bad),f)==sizeof(bad));
    assert(fclose(f)==0);
    assert(!avp_pc_eeprom_open_for_executable(NULL,NULL,0));
    f=fopen(path,"rb");assert(f);
    assert(fread(check,1,sizeof(check),f)==sizeof(check));
    assert(fgetc(f)==EOF);
    assert(fclose(f)==0);
    assert(memcmp(bad,check,sizeof(bad))==0);
    assert(unlink(path)==0);
    return 0;
}
