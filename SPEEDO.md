# SPEEDO main screen

Based on hardware-validated Base snapshot bfc78df. Uses the supplied main-screen
artwork with native LVGL MPH, Trip A and odometer values, and a red perimeter marker.
The silver bezel is cropped outside the 720 x 720 screen. The duplicate 70 in the
reference is covered; the remaining 0 through 80 labels are calibrated to the marker.

GAUGE_DEMO_MODE=1 remains enabled: 30-second 0..80..0 MPH sweep, Trip A 123.4 mi,
ODO 004532 mi. Demo values do not change persistent mileage.
With demo disabled, speed uses the existing GPS/CAN source and odometer uses
existing mileage storage. Trip A currently measures distance since boot; persistent
trip reset/control integration and CAN distance accumulation remain future work.

Build using ESP-IDF 5.5.6. The validated DSI compatibility fix is retained.
Full firmware build and on-device visual/performance checks are still required.
