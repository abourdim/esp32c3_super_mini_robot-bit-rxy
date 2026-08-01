#include "01_includes.h"

// ===========================================================================
events_connection_state_enum events_get_connect_flag( ) {
// ===========================================================================
  
  uint8_t l_rxy_connect_flag;

  l_rxy_connect_flag = remotexy_get_connect_flag();

  // Check for connection state changes
  if (l_rxy_connect_flag != event_connection_state_flag_bo) {
    if (l_rxy_connect_flag) {
      return EVENTS_CONNECTION_CONNECTED;
    } 

    return EVENTS_CONNECTION_DISCONNECTED;    
  }

  return EVENTS_CONNECTION_NO_CHANGE;
}

// ===========================================================================
void events_reset_connect_flag( ) {
// ===========================================================================
  event_connection_state_flag_bo = remotexy_get_connect_flag();  // Update state
}


// ===========================================================================
events_timeout_enum events_get_timeout_flag( ) {
// ===========================================================================
  
  uint32_t l_current = millis();

  if (l_current - g_previous_millis_u32 >= interval) {
    // g_previous_millis_u32 = l_current; 

    return EVENTS_TIMEOUT_OCCURED;
  }

  return EVENTS_TIMEOUT_NOT_OCCURED;
}

// ===========================================================================
void events_reset_timeout_flag( ) {
// ===========================================================================
  g_previous_millis_u32 = millis();  // Update state
}


