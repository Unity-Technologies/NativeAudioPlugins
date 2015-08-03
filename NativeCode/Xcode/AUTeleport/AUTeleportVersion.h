#ifndef __AUTeleportVersion_h__
#define __AUTeleportVersion_h__


#ifdef DEBUG
    #define kAUTeleportVersion 0xFFFFFFFF
#else
    #define kAUTeleportVersion 0x00010000
#endif

// customized for each audio unit
#define AUTeleport_COMP_SUBTYPE     'AUTP'
#define AUTeleport_COMP_MANF        'UNIT'

#endif
