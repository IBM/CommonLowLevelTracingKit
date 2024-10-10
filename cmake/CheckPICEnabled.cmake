
function(check_pic_enabled target_name)
  get_target_property(PIC_ENABLED ${target_name} POSITION_INDEPENDENT_CODE)
  if(NOT PIC_ENABLED)
    message(SEND_ERROR "PIC is not enabled for ${target_name}.")
  endif()
endfunction()
