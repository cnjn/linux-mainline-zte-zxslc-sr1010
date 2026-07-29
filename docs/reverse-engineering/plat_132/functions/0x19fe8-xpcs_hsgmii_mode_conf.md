# 0x19fe8 xpcs_hsgmii_mode_conf

Status: complete. Valid selectors configure PCS type one, 2.5G/VSMMD1 controls,
SR-MII speed three and duplex one, then wait for PSEQ with low power enabled.
Failure returns `-1`; success clears low power, configures PCS AN/timers, and
caches mode eight. On CPU 129/133, auto-enable one re-enables AN and sets the
