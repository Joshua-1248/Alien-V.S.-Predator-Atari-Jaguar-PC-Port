#include <assert.h>
#include <stdio.h>
#include "../health_policy.h"
/* Exercise the same signed-word health delta consumed by native Pain(). */
static int pain_delta(int16_t *e,int16_t *p){int delta=0;if(*e>0){delta=(int16_t)(*p-*e);*p=*e;}return delta;}
int main(void){
    unsigned n=0;
    for(int m=1;m<=300;m++)for(int damage=0;damage<=500;damage++){
        int16_t e=(int16_t)(m-damage),p=(int16_t)m;
        avp_new_health_apply(AVP_NEW_HEALTH_INFINITE,&e,&p,(int16_t)m);
        assert(e==m);assert(p==m+damage);
        avp_new_health_apply(AVP_NEW_HEALTH_INFINITE,&e,&p,(int16_t)m);
        avp_new_health_before_actor(AVP_NEW_HEALTH_INFINITE,&e,&p,(int16_t)m);
        assert(pain_delta(&e,&p)==damage);assert(pain_delta(&e,&p)==0);
        e=(int16_t)(m-damage);p=(int16_t)m;
        avp_new_health_apply(AVP_NEW_HEALTH_INVINCIBLE,&e,&p,(int16_t)m);
        assert(e==m-damage && p==m);
        avp_new_health_before_actor(AVP_NEW_HEALTH_INVINCIBLE,&e,&p,(int16_t)m);
        assert(e==m-damage && p==m);
        e=(int16_t)(m-damage);p=(int16_t)m;
        avp_new_health_apply(AVP_NEW_HEALTH_OFF,&e,&p,(int16_t)m);
        assert(e==m-damage&&p==m);n++;
    }
    {int16_t e=70,p=100;avp_new_health_apply(AVP_NEW_HEALTH_INFINITE,&e,&p,100);e-=12;avp_new_health_apply(AVP_NEW_HEALTH_INFINITE,&e,&p,100);assert(e==100&&pain_delta(&e,&p)==42);}
    {int16_t e=-32768,p=32767;avp_new_health_apply(AVP_NEW_HEALTH_INFINITE,&e,&p,100);assert(e==100&&p==32767);}
    {int16_t e=100,p=120,e2=50,p2=50;avp_new_health_apply(AVP_NEW_HEALTH_INVINCIBLE,&e2,&p2,50);assert(pain_delta(&e,&p)==20&&pain_delta(&e2,&p2)==0);}
    printf("PASS %u health cases; repeated refill, lethal damage, off mode, per-actor isolation, saturation\n",n);return 0;
}
