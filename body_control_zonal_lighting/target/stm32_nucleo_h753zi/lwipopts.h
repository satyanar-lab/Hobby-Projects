#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* ---- OS / threading ------------------------------------------------------ */
#define NO_SYS                          1
#define SYS_LIGHTWEIGHT_PROT            0

/* ---- Memory -------------------------------------------------------------- */
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (16U * 1024U)   /* 16 KB: was 8; +8 for TCP TX pbufs */
#define MEMP_NUM_UDP_PCB                4U
#define MEMP_NUM_PBUF                   24U             /* was 16; extra for TCP */
#define MEMP_NUM_TCP_PCB                2U              /* 1 connected + 1 spare */
#define MEMP_NUM_TCP_PCB_LISTEN         1U              /* DoIP diagnostic listener */
#define MEMP_NUM_TCP_SEG                16U             /* TX segment pool */

/* ---- Pbuf ---------------------------------------------------------------- */
#define PBUF_POOL_SIZE                  12U             /* was 8; TCP uses pool pbufs */
#define PBUF_POOL_BUFSIZE               1524U

/* ---- Protocol enables ---------------------------------------------------- */
#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define LWIP_UDP                        1
#define LWIP_TCP                        1
#define LWIP_ICMP                       1
#define LWIP_IGMP                       0
#define LWIP_DNS                        0
#define LWIP_DHCP                       0

/* ---- Sequential / socket API (incompatible with NO_SYS=1) ---------------- */
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0

/* ---- TCP tuning ---------------------------------------------------------- */
/* MSS: standard Ethernet minus IP+TCP headers (1500 - 20 - 20 = 1460).      */
#define TCP_MSS                         1460U
/* Send buffer per PCB — two segments is enough for our small UDS responses.  */
#define TCP_SND_BUF                     (2U * TCP_MSS)
/* Receive window — one segment is plenty; DoIP requests are < 50 bytes.      */
#define TCP_WND                         TCP_MSS
/* Disable out-of-order segment queuing to save memory (diagnostic channel).  */
#define TCP_QUEUE_OOSEQ                 0

/* ---- Timers (required for ARP aging, TCP retransmit, etc.) --------------- */
#define LWIP_TIMERS                     1

/* ---- ARP ----------------------------------------------------------------- */
/* ARP_TMR_INTERVAL is defined unconditionally in lwip/etharp.h (1000 ms) — */
/* it cannot be overridden from lwipopts.h and is intentionally omitted.     */
/*                                                                             */
/* ARP_MAXAGE: entry lifetime = ARP_MAXAGE × ARP_TMR_INTERVAL = 300 s.        */
/* This lwIP version (post-2016-08-23, ETHARP_TRUST_IP_MAC removed) refreshes  */
/* a stable entry automatically on every outbound packet once its ctime is    */
/* >= ARP_MAXAGE-30 (unicast) or >= ARP_MAXAGE-15 (broadcast).  Because the   */
/* rear node sends NodeHealthStatus every 1 s, the ctime can never reach the  */
/* renewal threshold without an immediate unicast renewal, so a stable entry  */
/* effectively never expires under normal traffic — provided etharp_tmr() is  */
/* driven exactly once per second by sys_check_timeouts() and not double-     */
/* counted from the main loop (see app/stm32_nucleo_h753zi/main.cpp).         */
#define ARP_TABLE_SIZE                  10U
#define ARP_MAXAGE                      300U
/* ARP_QUEUEING defaults to 0 in this lwIP port — packets sent while ARP is   */
/* pending are dropped rather than queued.  Enable queuing so the first event  */
/* after an ARP miss is buffered and delivered once the reply arrives.         */
#define ARP_QUEUEING                    1
#define ARP_QUEUE_LEN                   3U
#define ETHARP_SUPPORT_STATIC_ENTRIES   0

/* ---- Netif callbacks ----------------------------------------------------- */
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_LINK_CALLBACK        1

/* ---- Checksums (software; offload can be enabled later via HAL) ---------- */
#define LWIP_CHECKSUM_ON_COPY           1

/* ---- Stats (PBUF_POOL pool monitor for TX-stall diagnosis) --------------- */
#define LWIP_STATS                      1
#define LWIP_STATS_DISPLAY              0

/* ---- Debug --------------------------------------------------------------- */
#define LWIP_DEBUG                      0

#endif  /* LWIPOPTS_H */
