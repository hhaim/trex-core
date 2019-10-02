//#ifndef HAVE_TC_FLOWER
#define HAVE_TC_FLOWER 1
//#endif

//#ifndef HAVE_TC_VLAN_ID
//#undef  HAVE_TC_VLAN_ID
//#endif

//#ifndef HAVE_TC_BPF
#define HAVE_TC_BPF 1
//#endif

//HAVE_TC_BPF_FD
//#ifndef HAVE_TC_BPF_FD
//#define  HAVE_TC_BPF_FD 1
//#endif

#define HAVE_TC_VLAN_ID 1
//#ifndef HAVE_TC_ACT_BPF
//#undef HAVE_TC_ACT_BPF
//#endif

//#ifndef HAVE_TC_ACT_BPF_FD
//#undef HAVE_TC_ACT_BPF_FD
//#endif

//#include "linux/if_tun.h"

/* for old kernel*/
//#ifndef IFF_DETACH_QUEUE
//#define IFF_DETACH_QUEUE 0x0400
//#endif

//#ifndef TUNSETQUEUE
//#define TUNSETQUEUE _IOW('T', 217, int)
//#endif