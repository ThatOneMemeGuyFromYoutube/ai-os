#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "icmp.h"
#include "ipv4.h"
#define FRAME_SIZE 60u
#define ICMP_DATA "AsterOS ping"
static const uint8_t local_mac[6]={0x52,0x54,0,0x12,0x34,0x56};
static const uint8_t peer_mac[6]={0x52,0x54,0,0xAA,0xBB,0xCC};
static const uint8_t local_ip[4]={192,0,2,2};
static const uint8_t peer_ip[4]={192,0,2,1};
typedef struct {uint8_t frame[1514];uint16_t length;uint8_t calls;} capture_t;
static uint8_t transmit(void *c,const uint8_t *f,uint16_t n){
 capture_t *v=(capture_t *)c;assert(n<=sizeof v->frame);memcpy(v->frame,f,n);
 v->length=n;v->calls++;return 1;
}
static void put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static uint16_t get16(const uint8_t *p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static uint16_t build(uint8_t f[FRAME_SIZE]){
 uint8_t *ip,*icmp;uint16_t n=(uint16_t)(8u+sizeof(ICMP_DATA)-1u),total=(uint16_t)(20u+n);
 memset(f,0,FRAME_SIZE);memcpy(f,local_mac,6);memcpy(f+6,peer_mac,6);f[12]=8;f[13]=0;
 ip=f+14;ip[0]=0x45;put16(ip+2,total);put16(ip+4,0x1234);put16(ip+6,0x4000);
 ip[8]=64;ip[9]=1;memcpy(ip+12,peer_ip,4);memcpy(ip+16,local_ip,4);
 icmp=ip+20;icmp[0]=8;put16(icmp+4,0x4567);put16(icmp+6,1);
 memcpy(icmp+8,ICMP_DATA,sizeof(ICMP_DATA)-1u);
 put16(icmp+2,asteros_ipv4_checksum(icmp,n));put16(ip+10,asteros_ipv4_checksum(ip,20));
 return (uint16_t)(14u+total);
}
int main(void){
 asteros_netif_t i;capture_t c;uint8_t f[FRAME_SIZE],calls;
 uint16_t request_length,ip_length,icmp_length;const uint8_t *ip,*icmp;
 memset(&i,0,sizeof i);memset(&c,0,sizeof c);memcpy(i.name,"eth0",5);
 memcpy(i.mac,local_mac,6);i.mtu=ASTEROS_ETHERNET_MTU_MAX;i.link_up=1;
 i.driver_context=&c;i.transmit=transmit;asteros_net_init();
 assert(asteros_ipv4_init()==ASTEROS_IPV4_OK);assert(asteros_icmp_init()==ASTEROS_ICMP_OK);
 assert(asteros_icmp_init()==ASTEROS_ICMP_ALREADY_INITIALIZED);
 assert(asteros_netif_register(&i)==ASTEROS_NET_OK);
 assert(asteros_ipv4_configure(&i,local_ip)==ASTEROS_IPV4_OK);
 request_length=build(f);assert(asteros_netif_receive_frame(&i,f,request_length)==ASTEROS_NET_OK);
 assert(c.calls==1&&c.length==request_length);
 assert(memcmp(c.frame,peer_mac,6)==0&&memcmp(c.frame+6,local_mac,6)==0);
 assert(c.frame[12]==8&&c.frame[13]==0);ip=c.frame+14;ip_length=get16(ip+2);
 assert(ip[0]==0x45&&ip[8]==64&&ip[9]==1&&ip_length==request_length-14u);
 assert(asteros_ipv4_checksum(ip,20)==0&&memcmp(ip+12,local_ip,4)==0&&memcmp(ip+16,peer_ip,4)==0);
 icmp=ip+20;icmp_length=(uint16_t)(ip_length-20u);
 assert(icmp[0]==0&&icmp[1]==0&&get16(icmp+4)==0x4567&&get16(icmp+6)==1);
 assert(asteros_ipv4_checksum(icmp,icmp_length)==0);
 assert(memcmp(icmp+8,ICMP_DATA,sizeof(ICMP_DATA)-1u)==0);
 calls=c.calls;build(f);f[42]^=1;
 assert(asteros_netif_receive_frame(&i,f,request_length)==ASTEROS_NET_OK&&c.calls==calls);
 build(f);f[34]=3;put16(f+36,0);put16(f+36,asteros_ipv4_checksum(f+34,(uint16_t)(request_length-34u)));
 assert(asteros_netif_receive_frame(&i,f,request_length)==ASTEROS_NET_OK&&c.calls==calls);
 build(f);put16(f+16,27);put16(f+24,0);put16(f+24,asteros_ipv4_checksum(f+14,20));
 assert(asteros_netif_receive_frame(&i,f,41)==ASTEROS_NET_OK&&c.calls==calls);
 puts("ICMP echo: tests passed");return 0;
}
