# SPEEDO vacuum/boost secondary page

Main page remains the production startup default. Central MPH stays visible on
both pages. Boost page has no trip/odometer and uses the supplied artwork,
with a bright red 7px radial needle between the inner/outer blue arcs.
Vacuum uses -20..0 inHg, boost 0..20 PSI; zero is at the top. Gauge PSI input
is converted to inHg only for the vacuum half (1 PSI = 2.03602 inHg).

GAUGE_DEMO_MODE=1 alternates MAIN and BOOST every 30 seconds, with speed and
pressure test sweeps. This changes no stored mileage. With demo disabled there
is no automatic page switching. Call ui_speedo_set_page(UI_SPEEDO_PAGE_BOOST)
or UI_SPEEDO_PAGE_MAIN from LVGL context when wiring the control display.
ui_speedo_set_pressure() accepts signed gauge PSI, matching boost_psi from the
existing CAN mapping. That protocol's units/availability still need verification.

The display driver and DSI compatibility fix remain untouched. Full ESP-IDF
build and on-device visual/performance checks are pending.
