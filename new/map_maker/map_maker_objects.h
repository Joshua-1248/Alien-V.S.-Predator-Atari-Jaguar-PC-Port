#ifndef AVP_MAP_MAKER_OBJECTS_H
#define AVP_MAP_MAKER_OBJECTS_H
#include "map_maker.h"
/* Jaguar AMP placement coordinates use signed 8.8 and template IDs 0..59.
 * Groups follow AVP_AMP_LIST_{HUMAN,PREDATOR,ALIEN,COMMON} = 0,1,2,3. */
const char *avp_mm_object_name(unsigned def);
unsigned avp_mm_object_catalog_count(void);
unsigned avp_mm_object_catalog_def(unsigned id);
unsigned avp_mm_object_catalog_id(unsigned amp_def);
const char *avp_mm_object_catalog_name(unsigned id);
int avp_mm_object_place(AvpMapMaker *m,double x,double y);
int avp_mm_object_remove(AvpMapMaker *m,unsigned x,unsigned y);
int avp_mm_objects_save(const AvpMapMaker *m,const char *path);
int avp_mm_objects_load(AvpMapMaker *m,const char *path);
#endif
