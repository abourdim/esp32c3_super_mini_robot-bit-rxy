#ifndef __DEF_INCLUDE_EVENTS_H__
#define __DEF_INCLUDE_EVENTS_H__

typedef enum {
    EVENTS_CONNECTION_CONNECTED,      
    EVENTS_CONNECTION_DISCONNECTED,   
    EVENTS_CONNECTION_NO_CHANGE       
} events_connection_state_enum;

typedef enum {
    EVENTS_TIMEOUT_OCCURED,      
    EVENTS_TIMEOUT_NOT_OCCURED      
} events_timeout_enum;


extern events_connection_state_enum events_get_connect_flag( );
extern void events_reset_connect_flag( );

extern events_timeout_enum events_get_timeout_flag( );
extern void events_reset_timeout_flag( );

#endif // __DEF_INCLUDE_EVENTS_H__