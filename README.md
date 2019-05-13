# FeasycomModuleSDK
  Feasycom Module SDK now is available!
  
  Feasycom Bluetooth modules including BT826, BT836 are supported.
  
# Wiring
  |Host MCU|Bluetooth module|
  |---|----
  |BUART_RXD|BT_TXD
  |BUART_TXD|BT_RXD
  |GPIO_Input|Connection_State
    
# Optinal features
  * LED_Output, indicate the Bluetooth connection status. Define HAVE_LED to enable it.
  * HUART, for debugging. Define HAVE_HUART to enable it.
    
  
# Porting
  Files listed below need to be changed when porting to a new platform:
  * hal\hal.c
  * hal\hal.h
  * hal\hal_uart.c
  * hal\hal_uart.h

  Module specified parameters are defined in module/MODLUE_NAME/MODLUE_NAME.c. For example, module/bt826/bt826.c defines the parameters 
  that may only be applicable for BT826.
