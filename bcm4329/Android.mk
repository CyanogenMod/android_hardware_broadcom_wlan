ifeq ($(BOARD_WLAN_DEVICE),bcm4329)
    include $(call all-subdir-makefiles)
endif
ifeq ($(BOARD_WLAN_DEVICE),bcm4330)
    include $(call all-subdir-makefiles)
endif
