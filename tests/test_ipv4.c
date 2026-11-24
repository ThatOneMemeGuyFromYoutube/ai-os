#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ipv4.h"
#define FRAME_SIZE 60u
#define TEST_PROTOCOL 99u
static const uint8_t local_mac[6]={0x52,0x54,0,0x12,0x34,0x56};
static const uint8_t peer_mac[6]={0x52,0x54,0,0xAA,0xBB,0xCC};
static const uint8_t local_ip[4]={192,0,2,2};
static const uint8_t peer_ip[4]={192,0,2,1};
typedef struct {
    uint8_t calls, source_mac[6], destination_mac[6], source_ip[4], destination_ip[4];
    uint16_t payload_length;
    uint8_t payload[8];
} capture_t;
static uint8_t transmit(void *c,const uint8_t *f,uint16_t n){(void)c;(void)f;(void)n;return 1;}
static void handler(asteros_netif_t *i,const uint8_t *sm,const uint8_t *dm,
 const uint8_t *si,const uint8_t *di,const uint8_t *p,uint16_t n,void *c){
 capture_t *v=(capture_t *)c;(void)i;v->calls++;memcpy(v->source_mac,sm,6);
 memcpy(v->destination_mac,dm,6);memcpy(v->source_ip,si,4);memcpy(v->destination_ip,di,4);
 v->payload_length=n;if(n<=sizeof v->payload)memcpy(v->payload,p,n);
}
static void put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void rechecksum(uint8_t *frame,uint16_t header_length){
 frame[24]=0;frame[25]=0;put16(frame+24,asteros_ipv4_checksum(frame+14,header_length));
}
static void prepare(uint8_t f[FRAME_SIZE],uint8_t ihl,uint8_t proto){
 uint8_t *ip;uint16_t h=(uint16_t)(ihl*4u);memset(f,0,FRAME_SIZE);
 memcpy(f,local_mac,6);memcpy(f+6,peer_mac,6);f[12]=8;f[13]=0;ip=f+14;
 ip[0]=(uint8_t)(0x40u|ihl);put16(ip+2,(uint16_t)(h+4u));put16(ip+4,0x1234);
 put16(ip+6,0x4000);ip[8]=64;ip[9]=proto;memcpy(ip+12,peer_ip,4);
 memcpy(ip+16,local_ip,4);ip[h]=0xA1;ip[h+1]=0xB2;ip[h+2]=0xC3;ip[h+3]=0xD4;
 put16(ip+10,asteros_ipv4_checksum(ip,h));
}
static asteros_ipv4_result_t receive(asteros_netif_t *i,uint8_t *f,uint16_t n){
 return asteros_ipv4_receive(i,f+6,f,f+14,n);
}
int main(void){
 asteros_netif_t i;capture_t c;uint8_t f[FRAME_SIZE],other[4]={192,0,2,3};
 uint8_t badmac[6]={1,0,0x5E,0,0,1},n;
 memset(&i,0,sizeof i);memset(&c,0,sizeof c);memcpy(i.name,"eth0",5);
 memcpy(i.mac,local_mac,6);i.mtu=ASTEROS_ETHERNET_MTU_MAX;i.link_up=1;i.transmit=transmit;
 asteros_net_init();assert(asteros_ipv4_init()==ASTEROS_IPV4_OK);
 assert(asteros_ipv4_init()==ASTEROS_IPV4_ALREADY_INITIALIZED);
 assert(asteros_netif_register(&i)==ASTEROS_NET_OK);
 assert(asteros_ipv4_configure(&i,local_ip)==ASTEROS_IPV4_OK);
 assert(asteros_ipv4_register_protocol(TEST_PROTOCOL,handler,&c)==ASTEROS_IPV4_OK);
 assert(asteros_ipv4_register_protocol(TEST_PROTOCOL,handler,&c)==ASTEROS_IPV4_ADDRESS_IN_USE);
 prepare(f,5,TEST_PROTOCOL);assert(receive(&i,f,24)==ASTEROS_IPV4_OK);
 assert(c.calls==1&&c.payload_length==4&&c.payload[0]==0xA1);
 assert(memcmp(c.source_mac,peer_mac,6)==0&&memcmp(c.destination_mac,local_mac,6)==0);
 assert(memcmp(c.source_ip,peer_ip,4)==0&&memcmp(c.destination_ip,local_ip,4)==0);
 assert(asteros_netif_receive_frame(&i,f,sizeof f)==ASTEROS_NET_OK);
 assert(c.calls==2&&c.payload_length==4);
 prepare(f,6,TEST_PROTOCOL);f[34]=1;rechecksum(f,24);
 assert(receive(&i,f,28)==ASTEROS_IPV4_OK&&c.calls==3);
 prepare(f,5,TEST_PROTOCOL);assert(receive(&i,f,19)==ASTEROS_IPV4_PACKET_TOO_SHORT);
 prepare(f,5,TEST_PROTOCOL);f[14]=0x65;assert(receive(&i,f,24)==ASTEROS_IPV4_INVALID_VERSION);
 prepare(f,4,TEST_PROTOCOL);assert(receive(&i,f,24)==ASTEROS_IPV4_INVALID_HEADER_LENGTH);
 prepare(f,5,TEST_PROTOCOL);put16(f+16,10);assert(receive(&i,f,24)==ASTEROS_IPV4_INVALID_TOTAL_LENGTH);
 prepare(f,5,TEST_PROTOCOL);put16(f+16,25);assert(receive(&i,f,24)==ASTEROS_IPV4_INVALID_TOTAL_LENGTH);
 prepare(f,5,TEST_PROTOCOL);f[24]^=1;assert(receive(&i,f,24)==ASTEROS_IPV4_INVALID_CHECKSUM);
 prepare(f,5,TEST_PROTOCOL);f[22]=0;assert(receive(&i,f,24)==ASTEROS_IPV4_INVALID_TTL);
 prepare(f,5,TEST_PROTOCOL);put16(f+20,0x2000);rechecksum(f,20);
 assert(receive(&i,f,24)==ASTEROS_IPV4_FRAGMENTED_PACKET);
 prepare(f,5,TEST_PROTOCOL);memcpy(f+30,other,4);put16(f+24,asteros_ipv4_checksum(f+14,20));
 assert(receive(&i,f,24)==ASTEROS_IPV4_WRONG_DESTINATION);
 prepare(f,5,TEST_PROTOCOL);memset(f+26,0,4);put16(f+24,asteros_ipv4_checksum(f+14,20));
 assert(receive(&i,f,24)==ASTEROS_IPV4_INVALID_SOURCE);
 prepare(f,5,TEST_PROTOCOL);memcpy(f+6,badmac,6);
 assert(receive(&i,f,24)==ASTEROS_IPV4_INVALID_SOURCE);
 prepare(f,5,TEST_PROTOCOL);f[0]^=1;assert(receive(&i,f,24)==ASTEROS_IPV4_WRONG_DESTINATION);
 prepare(f,5,17);assert(receive(&i,f,24)==ASTEROS_IPV4_NO_HANDLER);
 assert(asteros_ipv4_checksum(0,1)==0&&asteros_ipv4_status_message(ASTEROS_IPV4_OK)!=0);
 for(n=0;n<ASTEROS_IPV4_MAX_PROTOCOL_HANDLERS-1u;n++)
  assert(asteros_ipv4_register_protocol((uint8_t)(n+10u),handler,&c)==ASTEROS_IPV4_OK);
 assert(asteros_ipv4_register_protocol(200,handler,&c)==ASTEROS_IPV4_TABLE_FULL);
 puts("IPv4 receive: tests passed");return 0;
}
