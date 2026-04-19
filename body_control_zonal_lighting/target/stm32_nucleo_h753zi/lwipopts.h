#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* ---- OS / threading ------------------------------------------------------ */
#define NO_SYS                          1
#define SYS_LIGHTWEIGHT_PROT            0

/* ---- Memory -------------------------------------------------------------- */
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (8U * 1024U)
#define MEMP_NUM_UDP_PCB                4U
#define MEMP_NUM_PBUF                   16U
#define MEMP_NUM_TCP_PCB                0U
#define MEMP_NUM_TCP_PCB_LISTEN         0U

/* ---- Pbuf ---------------------------------------------------------------- */
#define PBUF_POOL_SIZE                  8U
#define PBUF_POOL_BUFSIZE               1524U

/* ---- Protocol enables ---------------------------------------------------- */
#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define LWIP_UDP                        1
#define LWIP_TCP                        0
#define LWIP_ICMP                       1
#define LWIP_IGMP                       0
#define LWIP_DNS                        0
#define LWIP_DHCP                       0

/* ---- Sequential / socket API (incompatible with NO_SYS=1) ---------------- */
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0

/* ---- Timers (required for ARP aging, etc.) ------------------------------- */
#define LWIP_TIMERS                     1

/* ---- Netif callbacks ----------------------------------------------------- */
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_LINK_CALLBACK        1

/* ---- Checksums (software; offload can be enabled later via HAL) ---------- */
#define LWIP_CHECKSUM_ON_COPY           1

/* ---- Stats (disabled to save flash/RAM) ---------------------------------- */
#define LWIP_STATS                      0
#define LWIP_STATS_DISPLAY              0

/* ---- Debug --------------------------------------------------------------- */
#define LWIP_DEBUG                      0

#endif  /* LWIPOPTS_H */
