#include "oclock.h"
#include "hal.h"

#ifdef MASTER_MODE  
#include "time_tracker.h"

oclock::time_tracker::TestTimeTracker oclock::time_tracker::testTimeTracker;
#ifdef USE_TIME
oclock::time_tracker::RealTimeTracker oclock::time_tracker::realTimeTracker;
#endif
oclock::time_tracker::InternalClockTimeTracker oclock::time_tracker::internalClockTimeTracker;

#endif